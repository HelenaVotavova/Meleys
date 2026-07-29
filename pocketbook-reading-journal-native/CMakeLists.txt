cmake_minimum_required(VERSION 3.10)
project(HelcinCtenarskyDenik C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_path(INKVIEW_INCLUDE_DIR inkview.h HINTS
  "$ENV{SDK_BASE}/usr/arm-obreey-linux-gnueabi/sysroot/usr/local/include"
  "$ENV{SDK_ROOT}/usr/arm-obreey-linux-gnueabi/sysroot/usr/local/include")
find_library(INKVIEW_LIBRARY inkview HINTS
  "$ENV{SDK_BASE}/usr/arm-obreey-linux-gnueabi/sysroot/usr/local/lib"
  "$ENV{SDK_ROOT}/usr/arm-obreey-linux-gnueabi/sysroot/usr/local/lib")
find_path(FREETYPE_INCLUDE_DIR ft2build.h HINTS
  "$ENV{SDK_BASE}/usr/arm-obreey-linux-gnueabi/sysroot/usr/include/freetype2"
  "$ENV{SDK_ROOT}/usr/arm-obreey-linux-gnueabi/sysroot/usr/include/freetype2")

if(NOT INKVIEW_INCLUDE_DIR OR NOT INKVIEW_LIBRARY)
  message(FATAL_ERROR "PocketBook InkView SDK not found")
endif()

add_executable(HelcinCtenarskyDenik.app src/main.c)
target_include_directories(HelcinCtenarskyDenik.app PRIVATE "${INKVIEW_INCLUDE_DIR}")
if(FREETYPE_INCLUDE_DIR)
  target_include_directories(HelcinCtenarskyDenik.app PRIVATE "${FREETYPE_INCLUDE_DIR}")
endif()
target_link_libraries(HelcinCtenarskyDenik.app PRIVATE "${INKVIEW_LIBRARY}")
target_compile_definitions(HelcinCtenarskyDenik.app PRIVATE _GNU_SOURCE)
target_compile_options(HelcinCtenarskyDenik.app PRIVATE -Wall -Wextra -Wno-unused-parameter)
