#!/usr/bin/env bash
# Builds OpenWheels on Linux or macOS, or generates the iOS Xcode project (the POSIX twin of
# tools/build.ps1). Run tools/fetch_engine.sh first.
#
# Usage: tools/build.sh [options]
#   --config <Debug|Release|RelWithDebInfo>   build type (default RelWithDebInfo)
#   --build-dir <dir>                         build folder (default build-linux / build-macos / build-ios)
#   --ios                                     generate build-ios/OpenWheels.xcodeproj (device) and build it
#   --ios-simulator                           same, for the simulator (x86_64 / arm64 Macs via Rosetta)
#   --no-build                                configure only
#
# Output:
#   Linux:  build-linux/bin/OpenWheels/OpenWheels
#   macOS:  build-macos/bin/OpenWheels/<Config>/OpenWheels.app (or bin/OpenWheels/OpenWheels.app)
#   iOS:    build-ios/bin/OpenWheels/<Config>-iphoneos/OpenWheels.app (sign it in Xcode, or set
#           OW_IOS_TEAM=<your team id> before running this script)
set -euo pipefail

repo="$(cd "$(dirname "$0")/.." && pwd)"
config=RelWithDebInfo
build_dir=""
ios=0
simulator=0
build=1
while [ $# -gt 0 ]; do
    case "$1" in
        --config) config="$2"; shift ;;
        --build-dir) build_dir="$2"; shift ;;
        --ios) ios=1 ;;
        --ios-simulator) ios=1; simulator=1 ;;
        --no-build) build=0 ;;
        -h|--help) sed -n '2,17p' "$0"; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

if [ ! -f "$repo/thirdparty/cocos2d-x/cocos/cocos2d.h" ]; then
    echo "cocos2d-x not found: run tools/fetch_engine.sh first" >&2
    exit 1
fi

jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

if [ $ios -eq 1 ]; then
    [ "$(uname)" = Darwin ] || { echo "the iOS build needs macOS with Xcode" >&2; exit 1; }
    build_dir="${build_dir:-$repo/build-ios}"
    args=(-G Xcode -DCMAKE_TOOLCHAIN_FILE="$repo/thirdparty/cocos2d-x/cmake/ios.toolchain.cmake")
    [ $simulator -eq 1 ] && args+=(-DIOS_PLATFORM=SIMULATOR64)
    cmake -S "$repo" -B "$build_dir" "${args[@]}"
    if [ $build -eq 1 ]; then
        sdk=iphoneos
        [ $simulator -eq 1 ] && sdk=iphonesimulator
        cmake --build "$build_dir" --config "$config" -- -sdk "$sdk" -allowProvisioningUpdates
    fi
    echo "Xcode project: $build_dir/OpenWheels.xcodeproj"
    exit 0
fi

case "$(uname)" in
    Darwin) build_dir="${build_dir:-$repo/build-macos}" ;;
    *) build_dir="${build_dir:-$repo/build-linux}" ;;
esac

args=(-DCMAKE_BUILD_TYPE="$config")
if command -v ninja >/dev/null; then args+=(-G Ninja); fi
# The v3-deps-158 macOS prebuilts are x86_64 only: Apple Silicon Macs build an Intel app that runs
# under Rosetta 2.
[ "$(uname)" = Darwin ] && args+=(-DCMAKE_OSX_ARCHITECTURES=x86_64)
cmake -S "$repo" -B "$build_dir" "${args[@]}"
if [ $build -eq 1 ]; then
    cmake --build "$build_dir" --config "$config" --parallel "$jobs"
fi
