"""Run the oracle (original game in emulation) over every level listed in the player's
levelData.plist and store the loaded Box2D world for each: reports/oracle/levels/<file>.json.

  python tools/re/oracle_all_levels.py [--jobs 6] [--only REGEX]
"""
import argparse
import os
import plistlib
import re
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ASSETS = os.path.join(ROOT, "binary", "HappyWheels_Android", "HW_Android", "assets")
OUT = os.path.join(ROOT, "reports", "oracle", "levels")


def level_files():
    with open(os.path.join(ASSETS, "shared", "levels", "levelData.plist"), "rb") as f:
        pl = plistlib.load(f)
    files = []
    for ch in pl.get("chapters", []):
        for lv in ch.get("levels", []):
            df = lv.get("dataFile")
            if df and df not in files:
                files.append(df)
    return files


def run(df):
    name = re.sub(r"[^A-Za-z0-9_.-]+", "_", df)
    out = os.path.join(OUT, name.replace(".xml", ".json"))
    if os.path.exists(out):
        return df, "cached", 0.0
    t = time.time()
    p = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "re", "oracle.py"), "level",
                        "--level", "levels/" + df, "--out", out], capture_output=True, text=True)
    status = "ok" if p.returncode == 0 and os.path.exists(out) else "FAIL: " + (p.stderr.strip().splitlines() or ["?"])[-1]
    return df, status, time.time() - t


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--only", default=None)
    a = ap.parse_args()
    os.makedirs(OUT, exist_ok=True)
    files = [f for f in level_files() if not a.only or re.search(a.only, f)]
    print(f"{len(files)} levels")
    with ThreadPoolExecutor(a.jobs) as ex:
        for df, status, dt in ex.map(run, files):
            print(f"{status:8s} {dt:6.1f}s {df}", flush=True)


if __name__ == "__main__":
    main()
