#!/bin/sh
set -eu

if [ -z "${SDK_ROOT:-}" ]; then
  echo "Set SDK_ROOT to the PocketBook SDK directory." >&2
  echo "Example: SDK_ROOT=/path/to/SDK_6.3.0 ./build.sh" >&2
  exit 1
fi

TOOLCHAIN="${CMAKE_TOOLCHAIN_FILE:-$SDK_ROOT/share/cmake/arm_conf.cmake}"

if [ ! -f "$TOOLCHAIN" ]; then
  echo "Toolchain file not found: $TOOLCHAIN" >&2
  echo "Set CMAKE_TOOLCHAIN_FILE explicitly if your SDK uses a different path." >&2
  exit 1
fi

cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

echo "Built: build/MeleysSleepTimer.app"

