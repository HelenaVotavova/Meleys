#!/bin/sh
set -eu

SDK_ROOT="${SDK_ROOT:-${SDK_BASE:-}}"

if [ -z "$SDK_ROOT" ]; then
  echo "Set SDK_ROOT to the PocketBook SDK directory." >&2
  echo "Example: SDK_ROOT=/path/to/SDK_6.3.0 ./build.sh" >&2
  exit 1
fi

if [ -n "${CMAKE_TOOLCHAIN_FILE:-}" ]; then
  TOOLCHAIN="$CMAKE_TOOLCHAIN_FILE"
elif [ -f "$SDK_ROOT/config.cmake" ]; then
  TOOLCHAIN="$SDK_ROOT/config.cmake"
else
  TOOLCHAIN="$SDK_ROOT/share/cmake/arm_conf.cmake"
fi

if [ ! -f "$TOOLCHAIN" ]; then
  echo "Toolchain file not found: $TOOLCHAIN" >&2
  echo "Set CMAKE_TOOLCHAIN_FILE explicitly if your SDK uses a different path." >&2
  exit 1
fi

cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

echo "Built: build/MeleysSleepTimer.app"
