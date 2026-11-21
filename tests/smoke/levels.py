"""Test levels for tests/smoke/run_smoke.py (browser level format, written by hand or with
tools/levels/restored_lib.py). Nothing here comes from the site: every level is synthetic."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "levels"))
from restored_lib import Level  # noqa: E402


def finish_level() -> str:
    """Wheelchair Guy on flat ground with a finish line 1200 px ahead: holding Up wins."""
    lv = Level("finish.xml", "FINISH", char=1, start=(2000, 4930), bg=1)
    lv.box(800, 5000, 5600, 5400, color=0x6A8F3A)
    lv.finish(3200, 5000)
    return lv.xml()


def text_hide_level() -> str:
    """A red text hides (change opacity to 0) when the player's start overlaps a trigger with an 8 s delay."""
    lv = Level("text_hide.xml", "TEXT HIDE", char=1, start=(2000, 4930), bg=1)
    lv.box(800, 5000, 5600, 5400, color=0x6A8F3A)
    text = lv.text(1750, 4830, "HIDE ME", size=60, color=0xCC0000, font=3)
    lv.text(1750, 4930, "STAY", size=60, color=0x0000CC, font=3)
    lv.trigger(1800, 4700, 500, 500, targets=[(text, [(0, 0, 0)])], by=1, kind=1, repeat=1, delay=8)
    return lv.xml()


def click_hide_level() -> str:
    """A click trigger (b = 6) on a red box hides a text."""
    lv = Level("click_hide.xml", "CLICK HIDE", char=1, start=(2000, 4930), bg=1)
    lv.box(800, 5000, 5600, 5400, color=0x6A8F3A)
    text = lv.text(1750, 4830, "CLICK THE BOX", size=40, color=0xCC0000, font=3)
    lv.rect(2300, 4850, 300, 200, color=0x3333CC, fixed=True)
    lv.trigger(2300, 4850, 300, 200, targets=[(text, [(0, 0, 0)])], by=6, kind=1, repeat=1)
    return lv.xml()


def chain_video_level() -> str:
    """A miniature of the "Bad Apple!! 2" tricks that were broken in v0.4.2: a NaN start position
    (camera on the stage origin, character frozen and touching every sensor), a trigger whose
    interval is NaN (fires every frame), and trigger-to-trigger links with no action list.
    The root trigger starts a clock which fades a white rectangle in over a black screen."""
    return (
        '<levelXML><info v="1.95" x="NaN" y="5100" c="1" f="f" h="f" bg="0" bgc="8947848" e="1"/>'
        '<shapes>'
        '<sh t="0" p0="450" p1="250" p2="900" p3="500" p8="0" p10="100" i="f"/>'
        '<sh t="0" p0="450" p1="250" p2="900" p3="500" p8="16777215" p10="0" i="f"/>'
        '</shapes>'
        '<triggers>'
        '<t x="46" y="22" w="84" h="48" a="0" b="3" t="1" r="2" d="0"><t i="1"/></t>'
        '<t b="5" t="1" r="4" d="0" i="NaN"><t i="2"/></t>'
        '<t b="5" t="1" r="2" d="0"><sh i="1"><a i="3" p0="100" p1="0"/></sh></t>'
        '</triggers></levelXML>'
    )


LEVELS = {
    "finish": finish_level,
    "text_hide": text_hide_level,
    "click_hide": click_hide_level,
    "chain_video": chain_video_level,
}

if __name__ == "__main__":
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    for name, fn in LEVELS.items():
        (out / f"{name}.xml").write_text(fn(), encoding="utf-8")
        print(out / f"{name}.xml")
