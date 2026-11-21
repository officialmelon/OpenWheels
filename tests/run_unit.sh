#!/usr/bin/env bash
# Builds and runs the engine-free unit tests (tests/unit): the site protocol (level lists, replies,
# records, search matching) and the update check's version comparison. Needs g++, zlib and OpenSSL
# development headers (Debian / Ubuntu: apt install g++ zlib1g-dev libssl-dev). No engine, no assets.
#   tests/run_unit.sh [filter]     filter = part of a test name
set -euo pipefail
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/openwheels-unit-tests"
mkdir -p "$out"
g++ -std=c++14 -O1 -Wall -Wno-deprecated-declarations \
    -I src -I tests/vendor \
    tests/unit/*.cpp tests/vendor/tinyxml2/tinyxml2.cpp \
    src/online/Protocol.cpp src/qol/VersionCompare.cpp \
    -lz -lcrypto -o "$out/unit"
"$out/unit" "$@"
