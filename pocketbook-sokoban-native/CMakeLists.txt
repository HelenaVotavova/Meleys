cmake_minimum_required(VERSION 3.10)
project(HelcinSokoban C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_path(INKVIEW_INCLUDE_DIR inkview.h
  HINTS
    "$ENV{SDK_ROOT}/usr/arm-obreey-linux-gnueabi/sysroot/usr/local/include"
    "$ENV{SDK_ROOT}/usr/arm-obreey-linux-gnueabi/sysroot/usr/include"
    "$ENV{SDK_BASE}/usr/arm-obreey-linux-gnueabi/sysroot/usr/local/include"
    "$ENV{SDK_BASE}/usr/arm-obreey-linux-gnueabi/sysroot/usr/include"
)

find_library(INKVIEW_LIBRARY inkview
  HINTS
    "$ENV{SDK_ROOT}/usr/arm-obreey-linux-gnueabi/sysroot/usr/local/lib"
    "$ENV{SDK_ROOT}/usr/arm-obreey-linux-gnueabi/sysroot/usr/lib"
    "$ENV{SDK_BASE}/usr/arm-obreey-linux-gnueabi/sysroot/usr/local/lib"
    "$ENV{SDK_BASE}/usr/arm-obreey-linux-gnueabi/sysroot/usr/lib"
)

find_path(FREETYPE_INCLUDE_DIR ft2build.h
  HINTS
    "$ENV{SDK_ROOT}/usr/arm-obreey-linux-gnueabi/sysroot/usr/include/freetype2"
    "$ENV{SDK_ROOT}/usr/arm-obreey-linux-gnueabi/sysroot/usr/local/include/freetype2"
    "$ENV{SDK_BASE}/usr/arm-obreey-linux-gnueabi/sysroot/usr/include/freetype2"
    "$ENV{SDK_BASE}/usr/arm-obreey-linux-gnueabi/sysroot/usr/local/include/freetype2"
)

if(NOT INKVIEW_INCLUDE_DIR)
  message(FATAL_ERROR "inkview.h not found")
endif()

if(NOT INKVIEW_LIBRARY)
  message(FATAL_ERROR "libinkview not found")
endif()

add_executable(HelcinSokoban.app src/main.c)
target_include_directories(HelcinSokoban.app PRIVATE "${INKVIEW_INCLUDE_DIR}")
if(FREETYPE_INCLUDE_DIR)
  target_include_directories(HelcinSokoban.app PRIVATE "${FREETYPE_INCLUDE_DIR}")
endif()
target_link_libraries(HelcinSokoban.app PRIVATE "${INKVIEW_LIBRARY}")
target_compile_definitions(HelcinSokoban.app PRIVATE _GNU_SOURCE)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(HelcinSokoban.app PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

