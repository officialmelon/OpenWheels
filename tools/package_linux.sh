#!/usr/bin/env bash
# Builds the Linux release folder and archive: dist/OpenWheels-linux-x86_64/ and
# dist/OpenWheels-linux-x86_64.tar.gz (the POSIX twin of tools/package_windows.ps1).
#
#   1. Builds the Release configuration (tools/build.sh), which also generates gametext.tsv /
#      soundlist.tsv and the browser-game art under generated/ from the player's own files in binary/.
#   2. Copies the (stripped) executable, the .tsv tables, generated/ and the restored characters'
#      campaign levels (levels/restored).
#   3. Copies the game's assets (binary/HappyWheels_Android/HW_Android/assets) to assets/ and the iOS
#      bundle's resources (level-editor art, Localizable.strings) to ios/.
#   4. Adds LICENSE, NOTICE.md, README.txt and a .desktop file, then archives the folder.
#
# Usage: tools/package_linux.sh [--build-dir <dir>] [--no-build] [--out <dir>]
#                               [--assets <dir>] [--ios <dir>] [--data <dir>]
#   --data <dir>   take soundlist.tsv / gametext.tsv / generated/ from an existing OpenWheels
#                  package folder (e.g. a previous release) instead of the build output
set -euo pipefail

repo="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$repo/build-linux-release"
out="$repo/dist"
build=1
assets="$repo/binary/HappyWheels_Android/HW_Android/assets"
ios="$repo/binary/HappyWheels_iOS/Payload/happywheels.app"
data=""
while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir) build_dir="$2"; shift ;;
        --no-build) build=0 ;;
        --out) out="$2"; shift ;;
        --assets) assets="$2"; shift ;;
        --ios) ios="$2"; shift ;;
        --data) data="$2"; shift ;;
        -h|--help) sed -n '2,17p' "$0"; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

if [ $build -eq 1 ]; then
    "$repo/tools/build.sh" --config Release --build-dir "$build_dir"
fi
bin="$build_dir/bin/OpenWheels"
[ -x "$bin/OpenWheels" ] || { echo "no executable at $bin/OpenWheels" >&2; exit 1; }
[ -d "$assets/shared/levels" ] || { echo "game assets not found: $assets" >&2; exit 1; }
data="${data:-$bin}"

name=OpenWheels-linux-x86_64
dest="$out/$name"
rm -rf "$dest"
mkdir -p "$dest"

cp "$bin/OpenWheels" "$dest/OpenWheels"
strip --strip-debug "$dest/OpenWheels" 2>/dev/null || true
for t in soundlist.tsv gametext.tsv; do
    [ -f "$data/$t" ] && cp -L "$data/$t" "$dest/"
done
[ -d "$data/generated" ] && cp -rL "$data/generated" "$dest/generated"
if [ -d "$repo/res/levels/restored" ]; then
    mkdir -p "$dest/levels"
    cp -r "$repo/res/levels/restored" "$dest/levels/restored"
fi
cp -rL "$assets" "$dest/assets"
if [ -d "$ios" ]; then
    # Resources only (the editor's art and text); never an executable or frameworks.
    mkdir -p "$dest/ios"
    find -L "$ios" -maxdepth 1 -type f \( -name '*.png' -o -name '*.plist' -o -name '*.strings' -o -name '*.ttf' \
        -o -name '*.otf' -o -name '*.fnt' -o -name '*.xml' -o -name '*.json' \) ! -name Info.plist ! -name 'embedded*' \
        -exec cp {} "$dest/ios/" \;
fi

cp "$repo/LICENSE" "$repo/NOTICE.md" "$dest/"
cat > "$dest/README.txt" <<'EOF'
OpenWheels - an open-source reimplementation of Happy Wheels
https://github.com/officialmelon/OpenWheels

Run ./OpenWheels (Linux x86_64).

It needs the usual desktop libraries; on Debian / Ubuntu:
  sudo apt install libgtk-3-0 libglew2.2 libopenal1 libvorbisfile3 libmpg123-0 libcurl4 \
    libsqlite3-0 libfontconfig1 libpng16-16 libxrandr2 libxinerama1 libxcursor1 libxi6

Keyboard: Up/W accelerate, Down/S reverse, Left/A lean back, Right/D lean forward,
Space special action, Shift/Ctrl character actions, Z eject, Esc/P pause, R restart,
F11 fullscreen. Options -> "quality of life" has extra settings.
Saves and settings: ~/.config/OpenWheels/

Unofficial fan project, not affiliated with or endorsed by Fancy Force.
Happy Wheels and its assets belong to Fancy Force / Jim Bonacci. See NOTICE.md.
EOF
cat > "$dest/OpenWheels.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=OpenWheels
Comment=Open-source reimplementation of Happy Wheels
Exec=sh -c 'cd "$(dirname "$(readlink -f "%k")")" && ./OpenWheels'
Terminal=false
Categories=Game;
EOF
chmod +x "$dest/OpenWheels"

tar -C "$out" -czf "$out/$name.tar.gz" "$name"
echo "==> $dest"
echo "==> $out/$name.tar.gz ($(du -h "$out/$name.tar.gz" | cut -f1))"
