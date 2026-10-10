#!/usr/bin/env python3
"""make_dont_move_sample.py - writes a "don't move" browser level into a mock_tjf.py samples folder,
for the game's `--online-test dont-move` check (src/online/account/TjfTestDriver.h).

    python tools/online/make_dont_move_sample.py <samples dir> [--id 900001]
    python tools/online/mock_tjf.py --samples <samples dir> &
    OW_TJF_BASE=http://127.0.0.1:8765/ ./OpenWheels --online-test dont-move

The level: the Wheelchair Guy sits between two mines (any move blows him up) under a ramp. A ball
rolls down the ramp into a row of dominoes; the last one drops off the platform into a victory
trigger (any non-fixed shape), about 11 s in. A post with three boxes stands next to him: a
balance that must hold. Writes <id>.meta.xml and <id>.record.bin (Blowfish, as the site serves).
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "levels"))
sys.path.insert(0, str(REPO / "tools" / "online"))

from restored_lib import Level  # noqa: E402
from mock_tjf import bf_encrypt_record  # noqa: E402

AUTHOR = 4243  # the record's author id (part of its Blowfish key)


def level_xml() -> str:
    lv = Level("dont_move.xml", "DONT MOVE TEST", char=1, start=(2000, 4930), bg=1)
    lv.box(800, 5000, 5200, 5400, color=0x6a8f3a)  # ground
    # a ramp over the player down to a domino platform
    lv.slab(1100, 4150, 3000, 4450, thick=40, color=0x555555)
    lv.box(3000, 4450, 4000, 4490, color=0x555555)
    lv.circle(1180, 4080, 120, color=0xcc3333, fixed=False, density=4.0)
    for i in range(8):
        lv.rect(3210 + i * 105, 4350, 30, 200, color=0x3366cc, fixed=False, density=1.0)
    # a balanced stack next to the player: a post with three boxes on top
    lv.rect(2350, 4800, 40, 400, color=0x996633, fixed=False)
    for i in range(3):
        lv.rect(2350, 4570 - i * 60, 160, 60, color=0x999999, fixed=False, density=0.5)
    # mines on both sides: moving the wheelchair at all blows up
    lv.mine(1700, 5000)
    lv.mine(2650, 5000)
    # below the platform's end: the last domino falls into it
    lv.victory(4230, 4765, 440, 430, by=3)
    return lv.xml()


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("samples", type=Path)
    ap.add_argument("--id", type=int, default=900001)
    args = ap.parse_args()
    args.samples.mkdir(parents=True, exist_ok=True)
    (args.samples / f"{args.id}.record.bin").write_bytes(bf_encrypt_record(level_xml().encode(), AUTHOR))
    (args.samples / f"{args.id}.meta.xml").write_text(
        f'<lvs><lv id="{args.id}" ln="DONT MOVE TEST" ui="{AUTHOR}" un="OpenWheels" rg="4.5" vs="1" ps="1" '
        'dp="2026-10-10" dc="2026-10-10" pc="1"><uc>Do not touch a key.</uc></lv></lvs>')
    print(f"wrote {args.samples / str(args.id)}.meta.xml / .record.bin")


if __name__ == "__main__":
    main()
