#!/usr/bin/env bash
# iOS cannot decode Ogg Vorbis (the game's sound effects are .ogg; the music is .mp3), so the iPhone
# build was silent apart from the music. This makes an AAC copy (.m4a) next to every .ogg sound in a
# game-data folder; Sound::getFullPath uses the .m4a on iOS when it exists. Needs ffmpeg.
# Fails (exit 1) unless every .ogg ends up with an .m4a, so a half-converted package cannot ship.
#   tools/release/add_aac_sounds.sh game-data/assets
set -euo pipefail
root="${1:?usage: add_aac_sounds.sh <assets dir>}"
command -v ffmpeg >/dev/null || { echo "add_aac_sounds: ffmpeg not found" >&2; exit 2; }
total=0
made=0
while IFS= read -r -d '' f <&3; do
    total=$((total + 1))
    out="${f%.ogg}.m4a"
    if [ ! -s "$out" ]; then
        # -nostdin: ffmpeg must not read the file list from the loop's stdin (it ate every name after the first)
        ffmpeg -nostdin -loglevel error -y -i "$f" -c:a aac -b:a 96k "$out"
        made=$((made + 1))
    fi
done 3< <(find "$root" -name '*.ogg' -print0)
have=$(find "$root" -name '*.m4a' | wc -l)
echo "add_aac_sounds: $made converted, $have .m4a for $total .ogg under $root"
[ "$have" -ge "$total" ] || { echo "add_aac_sounds: missing AAC copies ($have of $total)" >&2; exit 1; }
