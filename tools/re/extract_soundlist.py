"""Generate the SoundList table from the player's own libMyGame.so.

SoundList::SoundList() in the original is one ~100 KB function that fills
  std::map<std::string, std::string>  sound name -> sound file base name   (+0x00)
  std::vector<std::string>            sound id   -> sound name             (+0x18)
Rather than transcribing that data into the repository, we execute the original constructor
in an arm64 emulator at build time and write the result next to the executable.

Output format (UTF-8, tab separated, one line per sound id, in id order):
  <id>\t<sound name>\t<file base name>
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu import Emu, DEFAULT_SO  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--so", default=DEFAULT_SO)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    if not os.path.exists(a.so):
        print(f"extract_soundlist: {a.so} not found; cannot build sound table", file=sys.stderr)
        return 1
    e = Emu(a.so)
    obj = e.malloc(0x100)
    e.call(e.sym_addr("_ZN9SoundListC1Ev"), obj)
    names = {}
    for n in e.std_map_nodes(obj):
        names[e.std_string(n + 0x20)] = e.std_string(n + 0x38)
    ids = [e.std_string(p) for p in e.std_vector(obj + 0x18, 0x18)]
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        for i, key in enumerate(ids):
            f.write(f"{i}\t{key}\t{names.get(key, '')}\n")
    print(f"extract_soundlist: {len(ids)} sounds -> {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
