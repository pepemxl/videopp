#!/usr/bin/env bash
# Build helper for Linux/macOS.
#   BUILD_DIR=build BUILD_TYPE=Release ./build.sh [extra cmake args...]
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

VCPKG_TOOLCHAIN=""
if [ -f "$SCRIPT_DIR/vcpkg/scripts/buildsystems/vcpkg.cmake" ]; then
    VCPKG_TOOLCHAIN="-DCMAKE_TOOLCHAIN_FILE=$SCRIPT_DIR/vcpkg/scripts/buildsystems/vcpkg.cmake"
fi

if command -v nproc >/dev/null 2>&1; then
    JOBS="$(nproc)"
elif command -v sysctl >/dev/null 2>&1; then
    JOBS="$(sysctl -n hw.ncpu)"
else
    JOBS=4
fi

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" $VCPKG_TOOLCHAIN "$@"
cmake --build "$BUILD_DIR" -j"$JOBS"

echo
echo "Built: $BUILD_DIR/bin/video_processor"
