#!/usr/bin/env bash
# Syntax/semantic check of game translation units against the real cocos2d-x 3.17.2 headers,
# using the Android NDK clang in the same configuration libMyGame.so was built with
# (aarch64-linux-android23, libc++). Usage: tools/check_tu.sh src/game/Foo.cpp [more.cpp ...]
#   OW_CHECK_EMIT_OBJ=1 tools/check_tu.sh ...   also emit build/arm64/<name>.o for parity diffing
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NDK="${ANDROID_NDK:-<home>/ndk-install/android-ndk-r27}"
CLANG="$NDK/toolchains/llvm/prebuilt/windows-x86_64/bin/clang++.exe"
E="$ROOT/thirdparty/cocos2d-x"
FLAGS=(--target=aarch64-linux-android23 -std=c++14 -DANDROID -DUSE_FILE32API -DCOCOS2D_DEBUG=0
       -fno-ms-compatibility -Wno-unused-command-line-argument -Wno-deprecated-declarations
       -Wno-overloaded-virtual -Wno-inconsistent-missing-override
       -I"$ROOT/src" -I"$ROOT/src/platform/stubs"
       -I"$E" -I"$E/cocos" -I"$E/cocos/platform/android" -I"$E/external"
       -I"$E/external/Box2D/include" -I"$E/external/freetype2/include/android/freetype2"
       -I"$E/extensions" -I"$E/cocos/editor-support")
GAME_DIRS=()
for d in "$ROOT"/src/game/*/; do GAME_DIRS+=(-I"${d%/}"); done
FLAGS+=("${GAME_DIRS[@]}")
status=0
for f in "$@"; do
  if [ "${OW_CHECK_EMIT_OBJ:-0}" = "1" ]; then
    mkdir -p "$ROOT/build/arm64"
    out="$ROOT/build/arm64/$(basename "${f%.*}").o"
    "$CLANG" "${FLAGS[@]}" -O2 -fno-exceptions -c "$f" -o "$out" || status=1
  else
    "$CLANG" "${FLAGS[@]}" -fsyntax-only "$f" || status=1
  fi
done
[ $status -eq 0 ] && echo "check_tu: OK ($#)" || echo "check_tu: FAILED"
exit $status
