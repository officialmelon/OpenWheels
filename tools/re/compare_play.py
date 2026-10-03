"""Behaviour parity sweep: original game (emulated oracle) vs our build, per level.

For every level: run `oracle.py play` and `OpenWheels.exe --dump-world` with the same
control-byte script for N frames, then diff the Box2D worlds (tools/re/worlddiff.py).

  python tools/re/compare_play.py [--frames 180] [--script "0:01,90:05,150:10"] [--only REGEX] [--jobs 5]
Results: reports/compare/<level>.{oracle,ours}_f<N>.json and reports/compare/summary_f<N>.txt
(the oracle cache is keyed by frame count only: delete it when changing --script for the same N)
"""
import argparse
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from oracle_all_levels import level_files  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "reports", "compare")
EXE = os.path.join(ROOT, "build", "bin", "OpenWheels", "RelWithDebInfo", "OpenWheels.exe")


def safe(df):
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", df).replace(".xml", "")


def run_oracle(df, frames, script):
    base = os.path.join(OUT, safe(df) + ".oracle.json")
    target = base.replace(".json", f"_f{frames}.json")
    if not os.path.exists(target):
        subprocess.run([sys.executable, os.path.join(HERE, "oracle.py"), "play", "--level", "levels/" + df,
                        "--frames", str(frames), "--script", script, "--dump-at", str(frames), "--out", base],
                       capture_output=True, text=True)
    return target if os.path.exists(target) else None


def run_ours(df, frames, script):
    out = os.path.join(OUT, safe(df) + f".ours_f{frames}.json")
    if os.path.exists(out):
        os.remove(out)
    subprocess.run([EXE, "--dump-world", out, "--level", "levels/" + df, "--frames", str(frames),
                    "--script", script], cwd=os.path.dirname(EXE), timeout=600)
    return out if os.path.exists(out) else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--frames", type=int, default=180)
    ap.add_argument("--script", default="0:01,90:05,150:10")
    ap.add_argument("--only", default=None)
    ap.add_argument("--jobs", type=int, default=5)
    ap.add_argument("--exe", default=None, help="OpenWheels.exe to test (default: build/bin/...)")
    a = ap.parse_args()
    global EXE
    if a.exe:
        EXE = os.path.abspath(a.exe)
    os.makedirs(OUT, exist_ok=True)
    files = [f for f in level_files() if not a.only or re.search(a.only, f)]
    with ThreadPoolExecutor(a.jobs) as ex:
        oracle = dict(zip(files, ex.map(lambda f: run_oracle(f, a.frames, a.script), files)))
    lines = []
    for df in files:
        o = oracle[df]
        u = run_ours(df, a.frames, a.script)
        if not o or not u:
            lines.append(f"ERROR    {df}  oracle={'ok' if o else 'FAIL'} ours={'ok' if u else 'FAIL'}")
        else:
            d = subprocess.run([sys.executable, os.path.join(HERE, "worlddiff.py"), o, u, "--max", "3"],
                               capture_output=True, text=True).stdout.strip().splitlines()
            status = "SAME" if d and d[-1].startswith("worlddiff: IDENTICAL") else "DIFF"
            lines.append(f"{status:8s} {df}  {d[-1] if d else ''}" + ("" if status == "SAME" else "  | " + " / ".join(d[:2])))
        print(lines[-1], flush=True)
    with open(os.path.join(OUT, f"summary_f{a.frames}.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
    same = sum(1 for l in lines if l.startswith("SAME"))
    print(f"compare_play: {same}/{len(lines)} levels identical after {a.frames} frames")


if __name__ == "__main__":
    main()
