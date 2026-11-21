#!/usr/bin/env bash
# iOS cannot decode Ogg Vorbis (the game's sound effects are .ogg; the music is .mp3), so the iPhone
# build was silent apart from the music. This makes an AAC copy (.m4a) next to every .ogg sound in a
# game-data folder; Sound::getFullPath uses the .m4a on iOS when it exists. Needs ffmpeg.
#   tools/release/add_aac_sounds.sh game-data/assets
set -euo pipefail
root="${1:?usage: add_aac_sounds.sh <assets dir>}"
command -v ffmpeg >/dev/null || { echo "add_aac_sounds: ffmpeg not found" >&2; exit 2; }
count=0
while IFS= read -r -d '' f; do
    out="${f%.ogg}.m4a"
    [ -f "$out" ] && continue
    ffmpeg -loglevel error -y -i "$f" -c:a aac -b:a 96k "$out"
    count=$((count + 1))
done < <(find "$root" -name '*.ogg' -print0)
echo "add_aac_sounds: $count .m4a files written under $root"
