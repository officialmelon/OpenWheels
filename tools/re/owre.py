#!/usr/bin/env python
"""OpenWheels reversal query tool.

Usage (all addresses are Ghidra addresses, i.e. file vaddr + 0x100000, as printed in the
decompilation; hex with or without 0x):

  owre.py fn <name-regex|addr> [--asm] [--max N]   decompiled body (+ disassembly) of function(s)
  owre.py callers <name-regex|addr>                who calls it (from the Ghidra call graph)
  owre.py data <addr> [--as f32|f64|u8|u16|u32|i32|u64|ptr|str] [--count N]
                                                 read constants / pointer tables / strings
  owre.py sym <addr>                               symbol at/containing an address
  owre.py vtable <Class>                           vtable slots: byte offset from vptr -> method
  owre.py vcall <Class> <byte-offset>              which method a `(*(vptr + off))()` call hits
  owre.py layout <EngineType>                      arm64 layout of a cocos2d-x/Box2D/std type
  owre.py fields <Class>                           every `this+off` access in the class's methods
  owre.py class <Class>                            bases, size, vtable, methods, statics summary
  owre.py methods <Class>                          per-method: addr, size, this/static, ghidra return
                                                 type, virtual slot (dups = inline copies)
  owre.py imm 0x70637768 0x70                      decode short-string immediates -> 'hwcpp'

Everything printed is derived from the original binary: use it to understand behaviour and
write NEW code - never paste it into src/.
"""
import argparse
import csv
import json
import os
import re
import struct
import sys
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from elfsyms import Elf, GHIDRA_BASE  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
SO = os.path.join(ROOT, "binary", "HappyWheels_Android", "config.arm64_v8a", "lib", "arm64-v8a",
                  "libMyGame.so")
# Per-function Ghidra JSON exports of libMyGame.so (<gaddr>.json); only `fn` bodies need them.
EXPORTS = os.environ.get("OW_EXPORTS", "")
DECOMP = os.path.join(ROOT, "reports", "decomp")

_elf = None


def elf():
    global _elf
    if _elf is None:
        _elf = Elf(SO).load()
    return _elf


def parse_addr(s):
    s = s.lower().replace("0x", "")
    if re.fullmatch(r"[0-9a-f]{5,}", s):
        return int(s, 16)
    return None


def all_functions():
    """[(ghidra_addr, demangled, size)] for every named function in the binary + game index."""
    e = elf()
    out = {}
    for s in e.syms:
        if s.kind == 2 and s.shndx and s.value:
            out.setdefault(s.value + GHIDRA_BASE, (s.demangled or s.name, s.size))
    idx = os.path.join(DECOMP, "functions.csv")
    if os.path.exists(idx):
        for r in csv.DictReader(open(idx, encoding="utf-8")):
            a = int(r["ghidra_addr"], 16)
            if a not in out:
                out[a] = (r["demangled"] or f"FUN_{a:08x}", int(r["size"] or 0))
    return out


def find_functions(query):
    a = parse_addr(query)
    funcs = all_functions()
    if a is not None:
        if a in funcs:
            return [(a, funcs[a][0])]
        # containing function
        best = None
        for fa, (nm, sz) in funcs.items():
            if fa <= a < fa + max(sz, 4) and (best is None or fa > best[0]):
                best = (fa, nm)
        return [best] if best else [(a, f"FUN_{a:08x}")]
    rx = re.compile(query)
    return sorted([(fa, nm) for fa, (nm, sz) in funcs.items() if rx.search(nm)])


def load_export(gaddr):
    if not EXPORTS or not os.path.isdir(EXPORTS):
        sys.exit("owre.py: set OW_EXPORTS to the folder of Ghidra JSON exports of libMyGame.so")
    p = os.path.join(EXPORTS, f"{gaddr:08x}.json")
    if not os.path.exists(p):
        return None
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def cmd_fn(args):
    hits = find_functions(args.query)
    if not hits:
        print("no match")
        return
    if len(hits) > args.max:
        print(f"{len(hits)} matches; showing names only (use --max):")
        for a, n in hits:
            print(f"  {a:08x}  {n}")
        return
    for a, n in hits:
        d = load_export(a)
        print("// " + "=" * 90)
        print(f"// {a:08x}  {n}")
        if not d:
            print("// (not in export)")
            continue
        print((d.get("decompiled") or "").replace("\r\n", "\n").strip())
        if args.asm:
            print("// ---- disassembly")
            for line in d.get("assembly") or []:
                print("//   " + line)


def cmd_callers(args):
    for a, n in find_functions(args.query)[:20]:
        d = load_export(a)
        print(f"{a:08x} {n}")
        for c in (d or {}).get("callers") or []:
            print("   <- " + (c if isinstance(c, str) else json.dumps(c)))


def describe_ptr(v):
    if not v:
        return "NULL"
    e = elf()
    s = e.name_of(v)
    if s:
        return f"{v + GHIDRA_BASE:08x} <{s.demangled or s.name}>"
    # inside some symbol?
    return f"{v + GHIDRA_BASE:08x}"


def cmd_data(args):
    e = elf()
    g = parse_addr(args.addr)
    va = g - GHIDRA_BASE
    kind, n = args.as_, args.count
    if kind == "str":
        raw = e.read(va, 4096) or b""
        print(repr(raw.split(b"\0")[0].decode("utf-8", "replace")))
        return
    if kind == "ptr":
        for i in range(n):
            tgt, sym = e.pointer_at(va + i * 8)
            label = (sym.demangled or sym.name) if sym is not None and tgt is None else describe_ptr(tgt)
            if tgt is not None and e.read(tgt, 1) is not None:
                # pointer to string?
                raw = e.read(tgt, 80) or b""
                txt = raw.split(b"\0")[0]
                if len(txt) >= 3 and all(32 <= c < 127 for c in txt):
                    label += f'  "{txt.decode()}"'
            print(f"[{i}] {g + i * 8:08x}: {label}")
        return
    fmt = {"f32": ("<f", 4), "f64": ("<d", 8), "u8": ("<B", 1), "u16": ("<H", 2), "u32": ("<I", 4),
           "i32": ("<i", 4), "u64": ("<Q", 8)}[kind]
    raw = e.read(va, fmt[1] * n)
    if raw is None:
        print("address not mapped (bss?)")
        return
    for i in range(n):
        v = struct.unpack_from(fmt[0], raw, i * fmt[1])[0]
        extra = f"  (0x{v:x})" if isinstance(v, int) else f"  (bits 0x{struct.unpack_from('<I' if fmt[1] == 4 else '<Q', raw, i * fmt[1])[0]:x})"
        print(f"[{i}] {g + i * fmt[1]:08x}: {v!r}{extra}")


def cmd_sym(args):
    for a, n in find_functions(args.addr):
        print(f"{a:08x} {n}")
    e = elf()
    g = parse_addr(args.addr)
    s = e.name_of(g - GHIDRA_BASE)
    if s:
        print("data/func symbol:", s.demangled or s.name, "size", s.size)


def load_classes():
    return json.load(open(os.path.join(DECOMP, "classes.json"), encoding="utf-8"))


def cmd_vtable(args):
    c = load_classes().get(args.cls)
    if not c:
        print("unknown class")
        return
    slots = c["vtable"]
    # Itanium: [offset-to-top][typeinfo] then function pointers; secondary vtables follow.
    print(f"vtable for {args.cls}: {len(slots)} words (vptr points at word 2 of each sub-table)")
    group_start = 0
    for i, s in enumerate(slots):
        if s.startswith("<offset") or s.startswith("typeinfo") or "typeinfo for" in s:
            print(f"  word {i:3d}: {s}")
            if "typeinfo" in s:
                group_start = i + 1
            continue
        print(f"  word {i:3d}  vptr+{(i - group_start) * 8:#05x}: {s}")


def cmd_vcall(args):
    c = load_classes().get(args.cls)
    off = int(args.off, 0)
    slots = c["vtable"]
    print(slots[2 + off // 8])


def cmd_layout(args):
    p = os.path.join(DECOMP, "engine_layouts.txt")
    text = open(p, encoding="utf-8").read()
    for block in text.split("*** "):
        first = block.split("\n", 1)[0]
        if re.search(r"\| (class|struct|union) " + re.escape(args.type) + r"( \(empty\))?$", first):
            print(block.strip())
            return
    print("not found; try the fully-qualified name, e.g. cocos2d::Sprite or b2Body")


FIELD_RX = re.compile(r"\*\((?P<type>[A-Za-z_0-9 ]+?\s*\**)\s*\)\s*\(\s*this\s*\+\s*(?P<off>0x[0-9a-fA-F]+|\d+)\s*\)"
                      r"|this\[(?P<boff>0x[0-9a-fA-F]+|\d+)\]")


def cmd_fields(args):
    cls = args.cls
    path = os.path.join(DECOMP, "classes", re.sub(r"[^A-Za-z0-9_]+", "_", cls) + ".c")
    if not os.path.exists(path):
        print("no decomp file for", cls)
        return
    text = open(path, encoding="utf-8").read()
    acc = defaultdict(lambda: defaultdict(set))
    cur = None
    for line in text.split("\n"):
        m = re.match(r"// (\S.*\(.*)$", line)
        if m and "::" in m.group(1) and not line.startswith("// _Z") and not line.startswith("// callees"):
            cur = m.group(1)
        for fm in FIELD_RX.finditer(line):
            if fm.group("off"):
                off = int(fm.group("off"), 0)
                ty = fm.group("type").strip()
            else:
                off = int(fm.group("boff"), 0)
                ty = "byte"
            acc[off][ty].add((cur or "?").split("(")[0])
    for off in sorted(acc):
        types = ", ".join(sorted(acc[off]))
        users = sorted({u for s in acc[off].values() for u in s})
        print(f"+{off:#06x}  {types:40s} {len(users)} fn: {', '.join(users[:6])}{' ...' if len(users) > 6 else ''}")


def cmd_methods(args):
    """Every function of a class: address, ghidra return type, call kind, virtual slot, size."""
    classes = load_classes()
    c = classes.get(args.cls, {})
    vt = c.get("vtable", [])
    slot_of = {}
    group_start = 0
    for i, s in enumerate(vt):
        if "typeinfo" in s:
            group_start = i + 1
            continue
        if s.startswith("<offset"):
            continue
        slot_of.setdefault(s, f"vptr+{(i - group_start) * 8:#x}")
    rows = [r for r in csv.DictReader(open(os.path.join(DECOMP, "functions.csv"), encoding="utf-8"))
            if r["class"] == args.cls and r["kind"] == "game"]
    seen = set()
    for r in rows:
        dem = r["demangled"]
        a = int(r["ghidra_addr"], 16)
        d = load_export(a) or {}
        body = (d.get("decompiled") or "").replace("\r\n", "\n")
        sig = ""
        for line in body.split("\n"):
            if "(" in line and not line.startswith("/*") and line.strip() and not line.startswith("  "):
                sig = line.strip()
                break
        kind = "this" if "__thiscall" in sig or re.search(r"\(\w+ \*this[,)]", sig) else "static?"
        ret = sig.split(args.cls + "::")[0].replace("__thiscall", "").strip() if args.cls + "::" in sig else "?"
        dup = " (dup)" if dem in seen else ""
        seen.add(dem)
        v = slot_of.get(dem, "")
        print(f"{a:08x} sz={int(r['size'] or 0):5d} {kind:7s} ret[{ret}] {('virtual ' + v) if v else ''} {dem}{dup}")


def cmd_imm(args):
    """Decode libc++ short-string immediates: little-endian byte order across the given words."""
    out = b""
    for w in args.words:
        v = int(w, 0)
        n = max(1, (v.bit_length() + 7) // 8)
        out += v.to_bytes(n, "little")
    print(repr(out.decode("latin-1")))


def cmd_class(args):
    c = load_classes().get(args.cls)
    if not c:
        print("unknown class")
        return
    print(json.dumps({k: v for k, v in c.items() if k != "vtable"}, indent=1))
    print(f"vtable words: {len(c['vtable'])} (use `owre.py vtable {args.cls}`)")


def main():
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("fn"); p.add_argument("query"); p.add_argument("--asm", action="store_true"); p.add_argument("--max", type=int, default=8); p.set_defaults(f=cmd_fn)
    p = sp.add_parser("callers"); p.add_argument("query"); p.set_defaults(f=cmd_callers)
    p = sp.add_parser("data"); p.add_argument("addr"); p.add_argument("--as", dest="as_", default="u32",
                                                                      choices=["f32", "f64", "u8", "u16", "u32", "i32", "u64", "ptr", "str"]); p.add_argument("--count", type=int, default=1); p.set_defaults(f=cmd_data)
    p = sp.add_parser("sym"); p.add_argument("addr"); p.set_defaults(f=cmd_sym)
    p = sp.add_parser("vtable"); p.add_argument("cls"); p.set_defaults(f=cmd_vtable)
    p = sp.add_parser("vcall"); p.add_argument("cls"); p.add_argument("off"); p.set_defaults(f=cmd_vcall)
    p = sp.add_parser("layout"); p.add_argument("type"); p.set_defaults(f=cmd_layout)
    p = sp.add_parser("fields"); p.add_argument("cls"); p.set_defaults(f=cmd_fields)
    p = sp.add_parser("class"); p.add_argument("cls"); p.set_defaults(f=cmd_class)
    p = sp.add_parser("methods"); p.add_argument("cls"); p.set_defaults(f=cmd_methods)
    p = sp.add_parser("imm"); p.add_argument("words", nargs="+"); p.set_defaults(f=cmd_imm)
    a = ap.parse_args()
    a.f(a)


if __name__ == "__main__":
    main()
