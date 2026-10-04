"""Static parity check: original libMyGame.so vs reconstructed source compiled for arm64.

For every function present in both, compares
  * the ordered list of direct call targets (bl), by symbol name, and
  * the set of float/double constants the function uses (fmov immediates + literal loads),
and reports what is missing or extra in the reconstruction. Inlining differences produce
some noise; a missing call or a missing constant is a strong hint that logic was dropped.

  python tools/re/parity.py build/arm64/*.o [--func REGEX] [--verbose]
Objects come from `OW_CHECK_EMIT_OBJ=1 bash tools/check_tu.sh src/game/Foo.cpp`.
"""
import argparse
import csv
import difflib
import os
import re
import struct
import subprocess
import sys
from collections import Counter

from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from elfsyms import Elf, GHIDRA_BASE  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SO = os.path.join(ROOT, "binary", "HappyWheels_Android", "config.arm64_v8a", "lib", "arm64-v8a", "libMyGame.so")


def _find_ndk():
    """Android NDK r27: $ANDROID_NDK / $ANDROID_NDK_HOME / $ANDROID_NDK_ROOT, else the copy
    tools/build_android.ps1 installs into the Android SDK (<sdk>/ndk/27.3.13750724)."""
    for var in ("ANDROID_NDK", "ANDROID_NDK_HOME", "ANDROID_NDK_ROOT"):
        if os.environ.get(var):
            return os.environ[var]
    sdk = (os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT")
           or os.path.join(os.environ.get("LOCALAPPDATA", ""), "Android", "Sdk"))
    ndk = os.path.join(sdk, "ndk", "27.3.13750724")
    if not os.path.isdir(ndk):
        sys.exit("parity.py: Android NDK r27 not found; set ANDROID_NDK to its folder")
    return ndk


NDK = _find_ndk()
OBJDUMP = os.path.join(NDK, "toolchains", "llvm", "prebuilt", "windows-x86_64", "bin", "llvm-objdump.exe")
CXXFILT = os.path.join(NDK, "toolchains", "llvm", "prebuilt", "windows-x86_64", "bin", "llvm-cxxfilt.exe")

NOISE = re.compile(r"^(operator new|operator delete|__stack_chk_fail|memset|memcpy|memmove|"
                   r"std::__ndk1::basic_string|std::__ndk1::__basic_string_common|"
                   r"std::__ndk1::__throw|__cxa_|_Unwind_|abort$)")


def norm(name):
    """Normalise a demangled callee name for comparison (drop params, ndk namespace)."""
    name = name.replace("std::__ndk1::", "std::").replace("(anonymous namespace)", "anon")
    name = re.sub(r"\[abi:[^\]]*\]", "", name)
    return name


class Original:
    def __init__(self):
        self.elf = Elf(SO).load()
        self.cs = Cs(CS_ARCH_ARM64, CS_MODE_ARM)
        self.cs.detail = False
        plt = self.elf.sections[".plt"]
        self.plt_lo, self.plt_hi = plt.addr, plt.addr + plt.size
        # map PLT stub address -> symbol via the GOT slot each stub loads
        self.plt_names = {}
        for ins in self.cs.disasm(self.elf.data[plt.offset:plt.offset + plt.size], plt.addr):
            pass
        self._map_plt()

    def _map_plt(self):
        plt = self.elf.sections[".plt"]
        code = self.elf.data[plt.offset:plt.offset + plt.size]
        insns = list(self.cs.disasm(code, plt.addr))
        for i in range(len(insns) - 2):
            a, b = insns[i], insns[i + 1]
            if a.mnemonic == "adrp" and b.mnemonic == "ldr":
                page = int(a.op_str.split("#")[-1], 16)
                m = re.search(r"#(0x[0-9a-f]+|\d+)\]", b.op_str)
                if not m:
                    continue
                slot = page + int(m.group(1), 0)
                rel = self.elf.relocs.get(slot)
                if rel and rel[1] is not None:
                    self.plt_names[a.address] = rel[1].demangled or rel[1].name

    def name_at(self, va):
        if self.plt_lo <= va < self.plt_hi:
            return self.plt_names.get(va, f"plt_{va:x}")
        s = self.elf.name_of(va)
        return (s.demangled or s.name) if s else f"FUN_{va + GHIDRA_BASE:08x}"

    def analyse(self, va, size):
        code = self.elf.data[self.elf.vaddr_to_off(va):self.elf.vaddr_to_off(va) + size]
        calls, consts = [], Counter()
        for ins in self.cs.disasm(code, va):
            if ins.mnemonic in ("bl", "b") and ins.op_str.startswith("#"):
                tgt = int(ins.op_str[1:], 16)
                if ins.mnemonic == "b" and va <= tgt < va + size:
                    continue  # intra-function branch
                calls.append(norm(self.name_at(tgt)))
            elif ins.mnemonic == "fmov" and "#" in ins.op_str:
                try:
                    consts[float(ins.op_str.split("#")[-1])] += 1
                except ValueError:
                    pass
            elif ins.mnemonic == "ldr" and re.match(r"^[sd]\d+, \[x\d+, #", ins.op_str):
                pass  # literal pool via adrp+ldr: resolved below
        # adrp/ldr literal loads of s/d registers
        prev = {}
        for ins in self.cs.disasm(code, va):
            if ins.mnemonic == "adrp":
                reg = ins.op_str.split(",")[0]
                prev[reg] = int(ins.op_str.split("#")[-1], 16)
            elif ins.mnemonic == "ldr" and re.match(r"^([sd])\d+, \[(x\d+), #(0x[0-9a-f]+|\d+)\]$", ins.op_str):
                m = re.match(r"^([sd])\d+, \[(x\d+), #(0x[0-9a-f]+|\d+)\]$", ins.op_str)
                kind, base, off = m.group(1), m.group(2), int(m.group(3), 0)
                if base in prev:
                    addr = prev[base] + off
                    raw = self.elf.read(addr, 8 if kind == "d" else 4)
                    if raw:
                        v = struct.unpack("<d" if kind == "d" else "<f", raw)[0]
                        consts[round(v, 6)] += 1
        return calls, consts


def analyse_object(path):
    """Parse llvm-objdump output of a reconstructed object: per function calls + constants."""
    out = subprocess.run([OBJDUMP, "-d", "-r", "--no-show-raw-insn", "-C", path],
                         capture_output=True, text=True, encoding="utf-8", errors="replace").stdout
    funcs = {}
    cur = None
    pending_reloc = None
    for line in out.split("\n"):
        m = re.match(r"^[0-9a-f]+ <(.+)>:$", line)
        if m:
            cur = m.group(1)
            funcs[cur] = ([], Counter())
            continue
        if cur is None:
            continue
        m = re.search(r"R_AARCH64_(CALL26|JUMP26)\s+(.+)$", line)
        if m:
            funcs[cur][0].append(norm(m.group(2).strip()))
            continue
        m = re.search(r"\bfmov\s+[sd]\d+, #(\S+)", line)
        if m:
            try:
                funcs[cur][1][float(m.group(1))] += 1
            except ValueError:
                pass
    return funcs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("objects", nargs="+")
    ap.add_argument("--func", default=None)
    ap.add_argument("--verbose", action="store_true")
    a = ap.parse_args()
    orig = Original()
    rows = list(csv.DictReader(open(os.path.join(ROOT, "reports", "decomp", "functions.csv"), encoding="utf-8")))
    by_name = {}
    for r in rows:
        if r["kind"] == "game" and r["demangled"]:
            by_name.setdefault(r["demangled"], r)
    total = clean = 0
    for obj in a.objects:
        for fname, (calls, consts) in analyse_object(obj).items():
            if a.func and not re.search(a.func, fname):
                continue
            r = by_name.get(fname)
            if not r:
                continue
            total += 1
            ocalls, oconsts = orig.analyse(int(r["vaddr"], 16), int(r["size"] or 0))
            oc = [c for c in ocalls if not NOISE.match(c)]
            nc = [c for c in calls if not NOISE.match(c)]
            missing = Counter(oc) - Counter(nc)
            extra = Counter(nc) - Counter(oc)
            okeys = {round(k, 5) for k in oconsts}
            nkeys = {round(k, 5) for k in consts}
            mconst = sorted(okeys - nkeys - {0.0})
            ratio = difflib.SequenceMatcher(None, oc, nc).ratio() if (oc or nc) else 1.0
            if not missing and not extra and not mconst:
                clean += 1
                if not a.verbose:
                    continue
            print(f"{r['ghidra_addr']} {fname}  seq={ratio:.2f}")
            for k, v in missing.items():
                print(f"    - missing call x{v}: {k}")
            for k, v in extra.items():
                print(f"    + extra call   x{v}: {k}")
            if mconst:
                print(f"    - fmov constants not seen in rebuild (may be literal-pool loads): {mconst[:12]}")
    print(f"parity: {clean}/{total} functions with identical call multiset & constants")


if __name__ == "__main__":
    main()
