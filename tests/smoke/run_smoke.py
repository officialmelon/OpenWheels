#!/usr/bin/env python3
"""Smoke tests that run the real game headlessly (Xvfb), play synthetic levels and check what is
on screen. They guard the behaviours that have broken between releases:

  finish       a normal level can be played and won (controls, physics, victory screen)
  text_hide    a trigger hides a browser text box (change opacity)
  click_hide   a click trigger fires (mouse input, b=6 triggers)
  chain_video  trigger chains with no action list, a NaN interval and a NaN start (Bad Apple!! 2)
  online_menu  the online level browser opens and reports an error instead of crashing offline
  online_pipeline  grab a level from a mock site and play it, then replay it (needs pycryptodome)

Usage:  tests/smoke/run_smoke.py --exe <path to OpenWheels> [--assets <dir>] [--only name,...] [--keep]
Needs: Xvfb, xdotool, ImageMagick's `import`, python3 with Pillow and numpy.
The game needs its assets (a release package's `assets/`, `generated/`, `levels/`, *.tsv next to it)."""
import argparse
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from levels import LEVELS  # noqa: E402

W, H = 1280, 720
DISPLAY = ":97"


def sh(*args, **kw):
    return subprocess.run(args, check=False, capture_output=True, text=True, env=dict(os.environ, DISPLAY=DISPLAY), **kw)


class Game:
    def __init__(self, exe: Path, workdir: Path, level: Path | None, extra=(), env_extra=None):
        self.log = workdir / f"{level.stem if level else 'menu'}.log"
        # Never touch the real network from tests: the site and the update check point at a dead local port.
        env = dict(os.environ, DISPLAY=DISPLAY, SDL_AUDIODRIVER="dummy", ALSOFT_DRIVERS="null",
                   OW_TJF_BASE="http://127.0.0.1:9/", OW_UPDATE_URL="http://127.0.0.1:9/latest")
        env.update(env_extra or {})
        cmd = [str(exe), "--width", str(W), "--height", str(H), "--console"]
        if level:
            cmd += ["--play-level", str(level)]
        cmd += list(extra)
        self.proc = subprocess.Popen(cmd, cwd=exe.parent, env=env, stdout=open(self.log, "w"), stderr=subprocess.STDOUT)
        self.dir = workdir

    def alive(self) -> bool:
        return self.proc.poll() is None

    def shot(self, name: str) -> np.ndarray:
        path = self.dir / f"{name}.png"
        sh("import", "-display", DISPLAY, "-window", "root", str(path))
        return np.asarray(Image.open(path).convert("RGB")).astype(int)

    def key(self, *args):
        sh("xdotool", *args)

    def stop(self):
        if self.alive():
            self.proc.send_signal(signal.SIGTERM)
            try:
                self.proc.wait(5)
            except subprocess.TimeoutExpired:
                self.proc.kill()

    def crashed(self) -> bool:
        text = self.log.read_text(errors="replace") if self.log.exists() else ""
        return "Segmentation fault" in text or "Assertion" in text and "failed" in text


def count(img, rgb, tol=40):
    return int((np.abs(img - np.array(rgb)).sum(axis=2) < tol).sum())


def wait_level(seconds=9):
    time.sleep(seconds)  # window + assets + level load


# ---- the tests: each returns None on success or a message --------------------------------------

def t_finish(g: Game):
    wait_level()
    g.key("keydown", "Up")
    banner = 0
    for _ in range(12):
        time.sleep(1.5)
        img = g.shot("finish")
        banner = count(img[100:460, 200:1080], (253, 129, 129), tol=60)  # the VICTORY banner
        if banner > 4000:
            break
    g.key("keyup", "Up")
    return None if banner > 4000 else f"no VICTORY banner after driving to the finish (pink pixels: {banner})"


def t_text_hide(g: Game):
    wait_level(5)
    before = g.shot("text_before")
    red = count(before, (204, 0, 0), tol=70)
    if red < 800:
        return f"the red text is not visible at the start (red pixels: {red})"
    time.sleep(9)
    after = g.shot("text_after")
    red_after = count(after, (204, 0, 0), tol=70)
    blue_after = count(after, (0, 0, 204), tol=70)
    if red_after > red * 0.1:
        return f"the text did not hide (red pixels {red} -> {red_after})"
    if blue_after < 800:
        return f"the other text disappeared too (blue pixels after: {blue_after})"
    return None


def t_click_hide(g: Game):
    wait_level(7)
    before = g.shot("click_before")
    red = count(before, (204, 0, 0), tol=70)
    if red < 800:
        return f"the text is not visible at the start (red pixels: {red})"
    # the box's centre is where the camera shows world (2450, 4950): found by its colour
    blue = np.argwhere(np.abs(before - np.array([51, 51, 204])).sum(axis=2) < 60)
    if len(blue) < 500:
        return "the click box is not visible"
    y, x = blue.mean(axis=0)
    g.key("mousemove", str(int(x)), str(int(y)), "click", "1")
    time.sleep(2)
    after = g.shot("click_after")
    red_after = count(after, (204, 0, 0), tol=70)
    return None if red_after < red * 0.1 else f"clicking the box did not hide the text (red {red} -> {red_after})"


def t_chain_video(g: Game):
    time.sleep(3)
    first = g.shot("chain_first")
    time.sleep(6)
    later = g.shot("chain_later")
    mid = lambda a: a[200:520, 300:980].mean()
    if not g.alive():
        return "the game quit"
    if mid(later) < 200:
        return (f"the trigger chain never faded the white rectangle in (centre brightness {mid(first):.0f} -> "
                f"{mid(later):.0f}); NaN start / NaN interval / action-less trigger links are broken")
    return None


def t_online_menu(g: Game):
    time.sleep(12)
    g.key("mousemove", "295", "630", "click", "1")  # the globe button of the main menu
    time.sleep(8)
    img = g.shot("online")
    # The online browser has a search field (a white pill) at the top; offline it shows an error,
    # online the list. Either way the game must still be running and drawn.
    if not g.alive():
        return "the game quit when opening Online Levels"
    top = img[20:100, 650:1080]
    return None if (top > 235).all(axis=2).mean() > 0.4 else "the Online Levels screen did not draw its search field"


def t_online_pipeline(g: Game):
    """Grab a level from a (mock) site and play it: list, download, Blowfish + zlib decryption, browser
    level conversion, play, replay of the run and its bit-for-bit comparison (--online-test dont-move)."""
    deadline = time.time() + 240
    while g.alive() and time.time() < deadline:
        time.sleep(2)
    if g.alive():
        return "the online scenario did not finish in 4 minutes"
    text = g.log.read_text(errors="replace")
    if "FINISHED ok" not in text:
        lines = [l for l in text.splitlines() if "FAIL" in l or "FINISHED" in l or "failed" in l.lower()]
        return "online scenario failed: " + " | ".join(l[:120] for l in lines[-4:])
    return None


TESTS = {
    "finish": (t_finish, "finish"),
    "text_hide": (t_text_hide, "text_hide"),
    "click_hide": (t_click_hide, "click_hide"),
    "chain_video": (t_chain_video, "chain_video"),
    "online_menu": (t_online_menu, None),
    "online_pipeline": (t_online_pipeline, None),
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True, type=Path)
    ap.add_argument("--only", default="")
    ap.add_argument("--keep", action="store_true", help="keep screenshots and logs (printed at the end)")
    ap.add_argument("--retries", type=int, default=0, help="run a failing test again this many times (rendering on CI runners is slow)")
    a = ap.parse_args()
    exe = a.exe.resolve()
    if not exe.exists():
        sys.exit(f"no such executable: {exe}")
    work = Path(tempfile.mkdtemp(prefix="ow-smoke-"))
    levels = work / "levels"
    from levels import LEVELS as L  # noqa: F811
    levels.mkdir()
    for name, fn in L.items():
        (levels / f"{name}.xml").write_text(fn(), encoding="utf-8")

    xvfb = None
    if not os.environ.get("OW_SMOKE_NO_XVFB"):
        xvfb = subprocess.Popen(["Xvfb", DISPLAY, "-screen", "0", f"{W}x{H}x24"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(2)

    wanted = [n for n in a.only.split(",") if n] or list(TESTS)
    failed = []
    for name in wanted:
        fn, level = TESTS[name]
        msg = "not run"
        for attempt in range(a.retries + 1):
            mock = None
            extra, env_extra = (), None
            if name == "online_pipeline":
                repo = Path(__file__).resolve().parents[2]
                samples = work / "samples"
                subprocess.run([sys.executable, str(repo / "tools/online/make_dont_move_sample.py"), str(samples)],
                               check=True, capture_output=True)
                mock = subprocess.Popen([sys.executable, str(repo / "tools/online/mock_tjf.py"), "--port", "8766",
                                         "--samples", str(samples)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                time.sleep(2)
                extra = ["--online-test", "dont-move"]
                env_extra = {"OW_TJF_BASE": "http://127.0.0.1:8766/", "OW_TJF_TEST_OUT": str(work)}
            game = Game(exe, work, levels / f"{level}.xml" if level else None, extra, env_extra)
            try:
                msg = fn(game)
                if msg is None and game.crashed():
                    msg = "the game crashed (see log)"
            except Exception as e:  # noqa: BLE001
                msg = f"test error: {e!r}"
            finally:
                game.stop()
                if mock:
                    mock.terminate()
            if msg is None:
                break
            if attempt < a.retries:
                print(f"  (retry {name}: {msg})")
        print(f"[{'FAIL' if msg else ' ok '}] {name}" + (f": {msg}" if msg else ""))
        if msg:
            failed.append(name)
            tail = game.log.read_text(errors="replace").splitlines()[-8:]
            print("      log tail: " + " | ".join(l[:100] for l in tail if "ALSA" not in l))
    if xvfb:
        xvfb.terminate()
    print(f"\n{len(wanted)} smoke tests, {len(failed)} failed" + (f"  (files in {work})" if a.keep or failed else ""))
    if not (a.keep or failed):
        shutil.rmtree(work, ignore_errors=True)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
