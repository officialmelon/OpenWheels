#!/usr/bin/env python3
"""Extract the game data the release workflow needs from an OpenWheels package.

The game files are never in the repository. The release workflow (.github/workflows/release.yml)
takes them from a previous OpenWheels release package instead - by default the Windows zip of the
latest GitHub release, which holds the player's assets plus the build-generated tables and art:

    <package>/assets/            the Android game's asset tree (shared/, sounds/, large/ ...)
    <package>/generated/         restored characters, browser-item art and sounds, kid gore
    <package>/ios/               iOS editor art and text (optional)
    <package>/soundlist.tsv, gametext.tsv

Usage: extract_game_data.py <package.zip|package.apk> <out-dir>

Writes <out-dir>/{assets,generated,ios,soundlist.tsv,gametext.tsv}. Zips made on Windows may store
paths with backslashes; they are normalised. An OpenWheels APK works too (its assets/ holds the
same files: the game tree plus generated/, ios/ and the tables).
"""

import os
import shutil
import sys
import tempfile
import zipfile

def extract(archive, dest):
    with zipfile.ZipFile(archive) as z:
        for info in z.infolist():
            name = info.filename.replace("\\", "/")
            parts = [p for p in name.split("/") if p not in ("", ".")]
            if not parts or ".." in parts or name.startswith("/"):
                continue
            target = os.path.join(dest, *parts)
            if name.endswith("/"):
                os.makedirs(target, exist_ok=True)
                continue
            os.makedirs(os.path.dirname(target), exist_ok=True)
            with z.open(info) as src, open(target, "wb") as out:
                shutil.copyfileobj(src, out)


DATA_ENTRIES = ("generated", "ios", "soundlist.tsv", "gametext.tsv")


def find_root(tree):
    """(release folder, its assets folder): the folder holding assets/shared/levels, or an APK."""
    for root, _dirs, _files in os.walk(tree):
        if os.path.isdir(os.path.join(root, "assets", "shared", "levels")):
            return root, os.path.join(root, "assets")
    return None, None


def data_path(root, assets, entry):
    """A data entry next to the assets (release folder) or inside them (APK)."""
    for base in (root, assets):
        path = os.path.join(base, entry)
        if os.path.exists(path):
            return path
    return None


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    archive, out = sys.argv[1], sys.argv[2]
    with tempfile.TemporaryDirectory() as tmp:
        extract(archive, tmp)
        root, assets = find_root(tmp)
        if not root:
            print("extract_game_data: no assets/shared/levels in %s" % archive, file=sys.stderr)
            return 1
        os.makedirs(out, exist_ok=True)
        out_assets = os.path.join(out, "assets")
        shutil.rmtree(out_assets, ignore_errors=True)
        os.makedirs(out_assets)
        for entry in os.listdir(assets):
            if entry in DATA_ENTRIES:
                continue  # an APK keeps the data inside assets/; it goes beside them here
            src = os.path.join(assets, entry)
            dst = os.path.join(out_assets, entry)
            (shutil.copytree if os.path.isdir(src) else shutil.copy2)(src, dst)
        for entry in DATA_ENTRIES:
            src = data_path(root, assets, entry)
            if not src:
                continue
            dst = os.path.join(out, entry)
            if os.path.isdir(src):
                shutil.rmtree(dst, ignore_errors=True)
                shutil.copytree(src, dst)
            else:
                shutil.copy2(src, dst)
    missing = [e for e in ("soundlist.tsv", "gametext.tsv") if not os.path.isfile(os.path.join(out, e))]
    if missing:
        print("extract_game_data: warning, missing %s (sounds / long texts will be missing)" % ", ".join(missing))
    print("extract_game_data: %s -> %s" % (archive, out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
