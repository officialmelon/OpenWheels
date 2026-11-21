#!/usr/bin/env bash
# tools/release/add_aac_sounds.sh must write an .m4a for EVERY .ogg (v0.5.2 shipped one: ffmpeg ate the
# file list from stdin). Makes a few tiny ogg files with ffmpeg and checks the counts. Needs ffmpeg.
set -euo pipefail
cd "$(dirname "$0")/.."
command -v ffmpeg >/dev/null || { echo "test_aac_script: ffmpeg not found, skipped"; exit 0; }
dir="$(mktemp -d)"
trap 'rm -rf "$dir"' EXIT
mkdir -p "$dir/sounds" "$dir/more"
for n in a b c d e; do
    ffmpeg -nostdin -loglevel error -f lavfi -i "sine=frequency=440:duration=0.2" -c:a libvorbis "$dir/sounds/$n.ogg"
done
ffmpeg -nostdin -loglevel error -f lavfi -i "sine=frequency=880:duration=0.2" -c:a libvorbis "$dir/more/z.ogg"
tools/release/add_aac_sounds.sh "$dir"
oggs=$(find "$dir" -name '*.ogg' | wc -l)
aacs=$(find "$dir" -name '*.m4a' | wc -l)
[ "$oggs" -eq 6 ] && [ "$aacs" -eq 6 ] || { echo "FAIL: $aacs m4a for $oggs ogg"; exit 1; }
# a second run changes nothing and still succeeds
tools/release/add_aac_sounds.sh "$dir" >/dev/null
echo "test_aac_script: ok ($aacs m4a for $oggs ogg)"
