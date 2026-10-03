#!/usr/bin/env python
"""Query tool for the iOS 1.2.7 build (original Objective-C codebase) - used to port the
level editor. Reads the per-class Ghidra exports (ns_<Class>.c, one "// ---- selector @ addr"
section per method) and the Mach-O ObjC metadata.

  iosre.py classes [regex]            ObjC classes with an export, with method counts
  iosre.py methods <Class>            selectors of a class (address order)
  iosre.py fn <Class> [selector-re]   decompiled body of matching methods
  iosre.py ivars <Class> [...]        instance variables (name, type, offset) from ObjC metadata
  iosre.py senders <selector-re>      classes/methods whose decompilation sends a selector
                                      (objc_stub::<sel> / Class::<sel> calls)

Exports: $OW_IOS_EXPORTS or ~/openwheels_old/ghidra/exports/happywheels.
Output is derived from the player's binary: read it to understand behaviour and write NEW code;
never paste it into src/.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
EXPORTS = os.environ.get("OW_IOS_EXPORTS",
                         os.path.expanduser("~/openwheels_old/ghidra/exports/happywheels"))
SECTION = re.compile(r"^// ---- (\S+) @ ([0-9a-f]+)\s*$", re.M)


def path(cls):
    return os.path.join(EXPORTS, f"ns_{cls}.c")


def sections(cls):
    p = path(cls)
    if not os.path.exists(p):
        sys.exit(f"no export for {cls}")
    text = open(p, encoding="utf-8", errors="replace").read()
    ms = list(SECTION.finditer(text))
    for i, m in enumerate(ms):
        end = ms[i + 1].start() if i + 1 < len(ms) else len(text)
        yield m.group(1), m.group(2), text[m.end():end].strip("\n")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    cmd, args = sys.argv[1], sys.argv[2:]
    if cmd == "classes":
        rx = re.compile(args[0]) if args else None
        for f in sorted(os.listdir(EXPORTS)):
            if not f.startswith("ns_") or not f.endswith(".c"):
                continue
            c = f[3:-2]
            if rx and not rx.search(c):
                continue
            n = sum(1 for _ in SECTION.finditer(open(os.path.join(EXPORTS, f), encoding="utf-8", errors="replace").read()))
            print(f"{n:5d}  {c}")
    elif cmd == "methods":
        for sel, addr, _ in sections(args[0]):
            print(f"{addr}  {sel}")
    elif cmd == "fn":
        rx = re.compile(args[1]) if len(args) > 1 else None
        for sel, addr, body in sections(args[0]):
            if rx and not rx.search(sel):
                continue
            print(f"// ==== -[{args[0]} {sel}] @ {addr}")
            print(body)
            print()
    elif cmd == "ivars":
        subprocess.run([sys.executable, os.path.join(HERE, "ios_ivars.py")] + args)
    elif cmd == "senders":
        rx = re.compile(r"(?:objc_stub|[A-Z]\w*)::" + args[0])
        for f in sorted(os.listdir(EXPORTS)):
            if not f.startswith("ns_"):
                continue
            c = f[3:-2]
            try:
                for sel, addr, body in sections(c):
                    if rx.search(body):
                        print(f"-[{c} {sel}] @ {addr}")
            except SystemExit:
                pass
    else:
        print(__doc__)


if __name__ == "__main__":
    main()
