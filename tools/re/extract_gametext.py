"""Generate gametext.tsv from the player's own copy of the game.

* OW_GAMETEXT(key, 0xADDR): ADDR is a Ghidra address in the Android libMyGame.so.
* OW_IOSTEXT(key, 0xADDR):  ADDR is an address in the iOS happywheels Mach-O - either the C
  string itself or the ObjC @"..." constant (__cfstring entry) that references it.

Scans src/ for those uses, reads each string from the binaries and writes `key<TAB>text`
lines (backslash, tab and newline escaped). Long UI text therefore never lives in the repo.
"""
import argparse
import glob
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from elfsyms import Elf, GHIDRA_BASE  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_SO = os.path.join(ROOT, "binary", "HappyWheels_Android", "config.arm64_v8a", "lib", "arm64-v8a",
                          "libMyGame.so")
DEFAULT_IOS = os.path.join(ROOT, "binary", "HappyWheels_iOS", "Payload", "happywheels.app", "happywheels")
ENTRY = re.compile(r"OW_GAMETEXT\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*,\s*(0x[0-9a-fA-F]+)\s*\)")
IOS_ENTRY = re.compile(r"OW_IOSTEXT\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*,\s*(0x[0-9a-fA-F]+)\s*\)")
NUL = b"\x00"


class MachO:
    """Minimal arm64 Mach-O reader: vmaddr -> bytes, C strings and ObjC @"..." constants."""

    def __init__(self, path):
        self.d = open(path, "rb").read()
        magic, _, _, _, ncmds = struct.unpack_from("<IiiII", self.d, 0)
        if magic != 0xFEEDFACF:
            raise ValueError("not a 64-bit Mach-O")
        off, self.segs, self.cfstring = 32, [], None
        for _ in range(ncmds):
            cmd, size = struct.unpack_from("<II", self.d, off)
            if cmd == 0x19:  # LC_SEGMENT_64
                vm, _vms, fo, fs = struct.unpack_from("<QQQQ", self.d, off + 24)
                nsects = struct.unpack_from("<I", self.d, off + 64)[0]
                self.segs.append((vm, fs, fo))
                so = off + 72
                for _ in range(nsects):
                    name = self.d[so:so + 16].rstrip(NUL)
                    addr, sz = struct.unpack_from("<QQ", self.d, so + 32)
                    if name == b"__cfstring":
                        self.cfstring = (addr, sz)
                    so += 80
            off += size

    def off(self, va):
        for vm, fs, fo in self.segs:
            if vm <= va < vm + fs:
                return fo + va - vm
        raise ValueError(f"address {va:#x} not mapped")

    def ptr(self, va):
        raw = struct.unpack_from("<Q", self.d, self.off(va))[0]
        if raw >> 36:  # chained-fixup rebase: low 36 bits are the target
            raw &= (1 << 36) - 1
            if raw < 0x100000000:
                raw += 0x100000000
        return raw

    def text(self, va):
        if self.cfstring and self.cfstring[0] <= va < self.cfstring[0] + self.cfstring[1]:
            cstr = self.ptr(va + 16)
            length = struct.unpack_from("<Q", self.d, self.off(va + 24))[0]
            o = self.off(cstr)
            return self.d[o:o + length].decode("utf-8", "replace")
        o = self.off(va)
        return self.d[o:self.d.index(NUL, o)].decode("utf-8", "replace")


def escape(s):
    return s.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n")


def collect():
    entries, ios_entries, conflicts = {}, {}, 0
    files = glob.glob(os.path.join(ROOT, "src", "**", "*.h"), recursive=True) + \
        glob.glob(os.path.join(ROOT, "src", "**", "*.cpp"), recursive=True)
    for path in files:
        text = open(path, encoding="utf-8", errors="replace").read()
        text = re.sub(r"#define\s+OW_(GAME|IOS)TEXT\b.*", "", text)
        for table, rx in ((entries, ENTRY), (ios_entries, IOS_ENTRY)):
            for key, addr in rx.findall(text):
                v = int(addr, 16)
                if key in table and table[key] != v:
                    print(f"extract_gametext: key '{key}' used with two addresses ({table[key]:#x}, {v:#x}) "
                          f"in {path}", file=sys.stderr)
                    conflicts += 1
                table[key] = v
    dup = set(entries) & set(ios_entries)
    for key in dup:
        print(f"extract_gametext: key '{key}' used by both OW_GAMETEXT and OW_IOSTEXT", file=sys.stderr)
    return entries, ios_entries, conflicts + len(dup)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--so", default=DEFAULT_SO)
    ap.add_argument("--ios", default=DEFAULT_IOS, help="iOS happywheels Mach-O (for OW_IOSTEXT)")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    if not os.path.exists(a.so):
        print(f"extract_gametext: {a.so} not found", file=sys.stderr)
        return 1
    entries, ios_entries, conflicts = collect()
    if conflicts:
        return 2
    elf = Elf(a.so).load()
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        for key in sorted(entries):
            raw = elf.read(entries[key] - GHIDRA_BASE, 8192) or b""
            f.write(f"{key}\t{escape(raw.split(NUL)[0].decode('utf-8', 'replace'))}\n")
        if ios_entries:
            if not os.path.exists(a.ios):
                print(f"extract_gametext: {a.ios} not found; {len(ios_entries)} iOS strings skipped",
                      file=sys.stderr)
            else:
                macho = MachO(a.ios)
                for key in sorted(ios_entries):
                    f.write(f"{key}\t{escape(macho.text(ios_entries[key]))}\n")
    print(f"extract_gametext: {len(entries)} Android + {len(ios_entries)} iOS strings -> {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
