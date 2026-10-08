#!/usr/bin/env bash
# Sets up the cocos2d-x 3.17.2 engine for OpenWheels in thirdparty/cocos2d-x (Linux, macOS; the
# iOS build uses the same engine folder). The POSIX twin of tools/fetch_engine.ps1:
#   1. fetches cocos2d-x tag cocos2d-x-3.17.2 and the prebuilt dependencies tag v3-deps-158
#      (shallow git clones; set OW_COCOS_URL / OW_DEPS_URL to use mirrors or local copies);
#   2. installs the dependencies the way the engine's download-deps.py does (external/ replaced,
#      config.json kept, fbx-conv moved to tools/);
#   3. applies thirdparty/patches/*.patch in name order (skipping already applied ones).
#
# Usage: tools/fetch_engine.sh [--force]
#   --force   delete and recreate thirdparty/cocos2d-x
#
# Linux also needs the system packages the engine links against, e.g. on Debian / Ubuntu:
#   sudo apt install build-essential cmake ninja-build python3 libx11-dev libxi-dev libxrandr-dev \
#     libxxf86vm-dev libxinerama-dev libxcursor-dev libfontconfig1-dev libgtk-3-dev zlib1g-dev \
#     libpng-dev libglew-dev libgl1-mesa-dev libcurl4-openssl-dev libsqlite3-dev libopenal-dev \
#     libvorbis-dev libmpg123-dev
set -euo pipefail

repo="$(cd "$(dirname "$0")/.." && pwd)"
thirdparty="$repo/thirdparty"
engine="$thirdparty/cocos2d-x"
staging="$thirdparty/_extract"
cocos_url="${OW_COCOS_URL:-https://github.com/cocos2d/cocos2d-x}"
deps_url="${OW_DEPS_URL:-https://github.com/cocos2d/cocos2d-x-3rd-party-libs-bin}"

step() { printf '\033[36m==> %s\033[0m\n' "$*"; }

force=0
for arg in "$@"; do
    case "$arg" in
        --force) force=1 ;;
        -h|--help) sed -n '2,18p' "$0"; exit 0 ;;
        *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done

command -v git >/dev/null || { echo "git is required" >&2; exit 1; }

install_engine() {
    rm -rf "$staging"
    mkdir -p "$staging"
    trap 'rm -rf "$staging"' EXIT
    step "fetching cocos2d-x 3.17.2 ($cocos_url)"
    GIT_LFS_SKIP_SMUDGE=1 git clone --quiet --depth 1 --branch cocos2d-x-3.17.2 "$cocos_url" "$staging/engine"
    step "fetching the v3-deps-158 prebuilt dependencies ($deps_url)"
    GIT_LFS_SKIP_SMUDGE=1 git clone --quiet --depth 1 --branch v3-deps-158 "$deps_url" "$staging/deps"
    rm -rf "$staging/engine/.git" "$staging/deps/.git"

    step "installing v3-deps-158 into external/ (download-deps.py layout)"
    local external="$staging/engine/external"
    find "$external" -mindepth 1 -maxdepth 1 ! -name config.json -exec rm -rf {} +
    find "$staging/deps" -mindepth 1 -maxdepth 1 ! -name config.json -exec mv {} "$external/" \;
    # config.json "move_dirs": external/fbx-conv -> tools/fbx-conv
    if [ -d "$external/fbx-conv" ]; then
        rm -rf "$staging/engine/tools/fbx-conv"
        mv "$external/fbx-conv" "$staging/engine/tools/fbx-conv"
    fi
    mv "$staging/engine" "$engine"
    rm -rf "$staging"
    trap - EXIT
}

install_patches() {
    shopt -s nullglob
    local patches=("$thirdparty"/patches/*.patch)
    if [ ${#patches[@]} -eq 0 ]; then
        step "no engine patches in thirdparty/patches"
        return
    fi
    cd "$repo"
    for p in "${patches[@]}"; do
        local rel
        rel="thirdparty/patches/$(basename "$p")"
        if git apply --check -p1 --directory=thirdparty/cocos2d-x "$rel" 2>/dev/null; then
            git apply -p1 --whitespace=nowarn --directory=thirdparty/cocos2d-x "$rel"
            step "applied $(basename "$p")"
        elif git apply --check --reverse -p1 --directory=thirdparty/cocos2d-x "$rel" 2>/dev/null; then
            step "already applied: $(basename "$p")"
        else
            echo "$rel neither applies nor is already applied (re-run with --force for a pristine engine)" >&2
            exit 1
        fi
    done
}

if [ -e "$engine" ]; then
    if [ $force -eq 1 ]; then
        step "removing existing $engine (--force)"
        rm -rf "$engine"
    else
        step "$engine already exists (use --force to recreate it)"
    fi
fi
[ -e "$engine" ] || install_engine
install_patches

if [ ! -f "$engine/cocos/cocos2d.h" ] || [ ! -d "$engine/external/Box2D/prebuilt" ]; then
    echo "engine setup incomplete: $engine" >&2
    exit 1
fi
step "engine ready: $engine"
