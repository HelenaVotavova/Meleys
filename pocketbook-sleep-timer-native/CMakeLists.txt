cmake_minimum_required(VERSION 3.10)
project(MeleysSleepTimer C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_path(INKVIEW_INCLUDE_DIR inkview.h
  HINTS
    "$ENV{SDK_ROOT}/include"
    "$ENV{SDK_ROOT}/PBSDK/include"
    "$ENV{SDK_ROOT}/usr/include"
    "$ENV{SDK_BASE}/include"
    "$ENV{SDK_BASE}/PBSDK/include"
    "$ENV{SDK_BASE}/usr/include"
)

find_library(INKVIEW_LIBRARY inkview
  HINTS
    "$ENV{SDK_ROOT}/lib"
    "$ENV{SDK_ROOT}/PBSDK/lib"
    "$ENV{SDK_ROOT}/usr/lib"
    "$ENV{SDK_BASE}/lib"
    "$ENV{SDK_BASE}/PBSDK/lib"
    "$ENV{SDK_BASE}/usr/lib"
)

if(NOT INKVIEW_INCLUDE_DIR)
  message(FATAL_ERROR "inkview.h not found. Set SDK_ROOT to your PocketBook SDK directory.")
endif()

if(NOT INKVIEW_LIBRARY)
  message(FATAL_ERROR "libinkview not found. Set SDK_ROOT to your PocketBook SDK directory.")
endif()

add_executable(MeleysSleepTimer.app src/main.c)
target_include_directories(MeleysSleepTimer.app PRIVATE "${INKVIEW_INCLUDE_DIR}")
target_link_libraries(MeleysSleepTimer.app PRIVATE "${INKVIEW_LIBRARY}")

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(MeleysSleepTimer.app PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
