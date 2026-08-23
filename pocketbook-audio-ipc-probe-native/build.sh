#!/bin/sh
set -eu
rm -rf build
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="${CMAKE_TOOLCHAIN_FILE:-/SDK/share/cmake/arm_conf.cmake}"
cmake --build build

