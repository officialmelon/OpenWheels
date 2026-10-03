"""Compare two Box2D world dumps (oracle = original game in emulation, ours = reconstruction).

Both sides emit the same JSON schema: bodies in b2World list order (reverse creation order),
fixtures in body list order, joints in world list order.

  python tools/re/worlddiff.py reports/oracle_x.json build/ours_x.json [--tol 1e-4] [--max 40]

Exit status 0 when identical within tolerance. --tol 0 compares bit-exactly as float32.
"""
import argparse
import json
import math
import struct
import sys


def close(a, b, tol):
    if isinstance(a, list) and isinstance(b, list):
        return len(a) == len(b) and all(close(x, y, tol) for x, y in zip(a, b))
    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        if isinstance(a, float) and math.isnan(a) and isinstance(b, float) and math.isnan(b):
            return True
        if tol == 0:  # exact: both sides are float32 values (ours printed with 9 digits)
            return struct.unpack("<f", struct.pack("<f", a)) == struct.unpack("<f", struct.pack("<f", b))
        return abs(a - b) <= tol * max(1.0, abs(a), abs(b))
    return a == b


def diff_obj(path, a, b, tol, out):
    keys = sorted(set(a) | set(b))
    for k in keys:
        if k in ("fixtures",):
            continue
        if k not in a or k not in b:
            out.append(f"{path}.{k}: missing on {'oracle' if k not in a else 'ours'}")
        elif not close(a[k], b[k], tol):
            out.append(f"{path}.{k}: oracle={a[k]} ours={b[k]}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("oracle")
    ap.add_argument("ours")
    ap.add_argument("--tol", type=float, default=1e-4)
    ap.add_argument("--max", type=int, default=40)
    a = ap.parse_args()
    A = json.load(open(a.oracle))
    B = json.load(open(a.ours))
    out = []
    if not close(A.get("gravity"), B.get("gravity"), a.tol):
        out.append(f"gravity: oracle={A.get('gravity')} ours={B.get('gravity')}")
    na, nb = len(A["bodies"]), len(B["bodies"])
    if na != nb:
        out.append(f"body count: oracle={na} ours={nb}")
    for i, (x, y) in enumerate(zip(A["bodies"], B["bodies"])):
        diff_obj(f"body[{i}]", x, y, a.tol, out)
        fa, fb = x.get("fixtures", []), y.get("fixtures", [])
        if len(fa) != len(fb):
            out.append(f"body[{i}].fixtures: count oracle={len(fa)} ours={len(fb)}")
        for j, (p, q) in enumerate(zip(fa, fb)):
            diff_obj(f"body[{i}].fixture[{j}]", p, q, a.tol, out)
    ja, jb = len(A["joints"]), len(B["joints"])
    if ja != jb:
        out.append(f"joint count: oracle={ja} ours={jb}")
    for i, (x, y) in enumerate(zip(A["joints"], B["joints"])):
        diff_obj(f"joint[{i}]", x, y, a.tol, out)
    if not out:
        print(f"worlddiff: IDENTICAL ({na} bodies, {ja} joints, tol={a.tol})")
        return 0
    for line in out[:a.max]:
        print(line)
    if len(out) > a.max:
        print(f"... {len(out) - a.max} more differences")
    print(f"worlddiff: {len(out)} differences")
    return 1


if __name__ == "__main__":
    sys.exit(main())
