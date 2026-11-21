#!/usr/bin/env python3
"""Fails when the version is not the same everywhere it is written: src/qol/AppVersion.h (compiled
into the game, used by the update check), android/app/build.gradle (versionName, versionCode) and the
newest `## vX.Y.Z` heading of CHANGELOG.md. An empty or stale AppVersion.h once broke a build, and a
version bump that forgets one of them breaks the in-game update prompt. Optional: pass the release
tag (vX.Y.Z) to also check it matches."""
import re
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
errors = []


def read(rel):
    p = root / rel
    if not p.exists() or not p.read_text(encoding="utf-8").strip():
        errors.append(f"{rel} is missing or empty")
        return ""
    return p.read_text(encoding="utf-8")


header = read("src/qol/AppVersion.h")
m = re.search(r'#define\s+OW_APP_VERSION\s+"(\d+\.\d+\.\d+)"', header)
app = m.group(1) if m else None
if not app:
    errors.append('src/qol/AppVersion.h has no  #define OW_APP_VERSION "X.Y.Z"')

gradle = read("android/app/build.gradle")
m = re.search(r"versionName\s*=\s*betaBuild\s*\?\s*\(owProp\('OW_BETA_VERSION'\)\s*\?:\s*'([^']+)'\)\s*:\s*'([^']+)'", gradle)
if not m:
    errors.append("android/app/build.gradle: versionName line not recognised")
else:
    beta, release = m.groups()
    if release != app:
        errors.append(f"build.gradle versionName {release} != AppVersion.h {app}")
    if beta != f"{app}-beta":
        errors.append(f"build.gradle beta versionName {beta} != {app}-beta")
codes = re.search(r"versionCode\s*=\s*betaBuild\s*\?\s*(\d+)\s*:\s*(\d+)", gradle)
if not codes:
    errors.append("android/app/build.gradle: versionCode line not recognised")
elif int(codes.group(1)) != int(codes.group(2)) + 1:
    errors.append(f"versionCode beta {codes.group(1)} should be release {codes.group(2)} + 1")

changelog = read("CHANGELOG.md")
m = re.search(r"^## v(\d+\.\d+\.\d+)\s*$", changelog, re.M)
if not m:
    errors.append("CHANGELOG.md has no '## vX.Y.Z' heading")
elif m.group(1) != app:
    errors.append(f"newest CHANGELOG section v{m.group(1)} != AppVersion.h {app}")

if len(sys.argv) > 1:
    tag = sys.argv[1].lstrip("v")
    if tag != app:
        errors.append(f"release tag v{tag} != AppVersion.h {app}")

if errors:
    print("release consistency check FAILED:")
    for e in errors:
        print("  -", e)
    sys.exit(1)
print(f"release consistency ok: {app}")
