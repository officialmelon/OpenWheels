"""Generate gametext.tsv from the player's own libMyGame.so.

Reads the OW_GAMETEXT(key, 0xADDR) entries declared in src/game/GameText.h (and any
src/game/*.cpp/*.h using the same macro form), pulls each NUL-terminated string from the
original library at that Ghidra address, and writes `key<TAB>escaped text` lines.
Long UI text therefore never lives in the repository.
"""
import argparse
import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from elfsyms import Elf, GHIDRA_BASE  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_SO = os.path.join(ROOT, "binary", "HappyWheels_Android", "config.arm64_v8a", "lib", "arm64-v8a",
                          "libMyGame.so")
ENTRY = re.compile(r"OW_GAMETEXT\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*,\s*(0x[0-9a-fA-F]+)\s*\)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--so", default=DEFAULT_SO)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    if not os.path.exists(a.so):
        print(f"extract_gametext: {a.so} not found", file=sys.stderr)
        return 1
    entries = {}
    conflicts = 0
    for path in glob.glob(os.path.join(ROOT, "src", "**", "*.h"), recursive=True) + \
            glob.glob(os.path.join(ROOT, "src", "**", "*.cpp"), recursive=True):
        text = open(path, encoding="utf-8", errors="replace").read()
        text = re.sub(r"#define\s+OW_GAMETEXT\b.*", "", text)
        for key, addr in ENTRY.findall(text):
            addr_val = int(addr, 16)
            if key in entries and entries[key] != addr_val:
                print(f"extract_gametext: key '{key}' used with two addresses "
                      f"({entries[key]:#x}, {addr_val:#x}) in {path}", file=sys.stderr)
                conflicts += 1
            entries[key] = addr_val
    if conflicts:
        return 2
    elf = Elf(a.so).load()
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        for key in sorted(entries):
            raw = elf.read(entries[key] - GHIDRA_BASE, 8192) or b""
            s = raw.split(b"\0")[0].decode("utf-8", "replace")
            s = s.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n")
            f.write(f"{key}\t{s}\n")
    print(f"extract_gametext: {len(entries)} strings -> {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
