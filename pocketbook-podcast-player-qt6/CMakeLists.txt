cmake_minimum_required(VERSION 3.21)
project(helciny-podcasty-qt6 C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(pocketbook_qt6
  GIT_REPOSITORY https://github.com/fstanis/pocketbook-sdk-qt6.git
  GIT_TAG main
)
FetchContent_MakeAvailable(pocketbook_qt6)

add_library(Qt6::Multimedia SHARED IMPORTED GLOBAL)
set_target_properties(Qt6::Multimedia PROPERTIES
  IMPORTED_LOCATION "${POCKETBOOK_TARGET_LIB_DIR}/libQt6Multimedia.so.6"
  INTERFACE_INCLUDE_DIRECTORIES
    "${POCKETBOOK_QT_INCLUDE_DIR};${POCKETBOOK_QT_INCLUDE_DIR}/QtMultimedia"
)

pocketbook_add_app(HelcinyPodcastyQt
  SOURCES src/main.cpp src/audio_test.cpp
  QRC qml/app.qrc
  LIBRARIES Qt6::Multimedia
)
set_target_properties(HelcinyPodcastyQt PROPERTIES OUTPUT_NAME "HelcinyPodcastyQt.bin")

pocketbook_add_app(HelcinyPodcastyQtLauncher
  SOURCES src/launcher.cpp
)
set_target_properties(HelcinyPodcastyQtLauncher PROPERTIES OUTPUT_NAME "HelcinyPodcastyQt.app")

add_executable(HelcinyAudioWorker src/audio_worker.cpp)
set_target_properties(HelcinyAudioWorker PROPERTIES OUTPUT_NAME "HelcinyAudioWorker.bin" SUFFIX "")
target_link_libraries(HelcinyAudioWorker PRIVATE Qt6::Multimedia Qt6::Core)

add_executable(AudioWorkerTest src/audio_worker_launcher.cpp)
set_target_properties(AudioWorkerTest PROPERTIES OUTPUT_NAME "AudioWorkerTest.app" SUFFIX "")
target_link_libraries(AudioWorkerTest PRIVATE PocketBook::InkView)
target_include_directories(AudioWorkerTest PRIVATE "${POCKETBOOK_SDK_INCLUDE_DIR}" "/usr/include/freetype2")

add_library(ALSA::ALSA SHARED IMPORTED)
set_target_properties(ALSA::ALSA PROPERTIES
  IMPORTED_LOCATION "/usr/lib/arm-linux-gnueabi/libasound.so"
  INTERFACE_INCLUDE_DIRECTORIES "/usr/include"
)
add_library(MPG123::MPG123 SHARED IMPORTED)
set_target_properties(MPG123::MPG123 PROPERTIES
  IMPORTED_LOCATION "/usr/lib/arm-linux-gnueabi/libmpg123.so"
  INTERFACE_INCLUDE_DIRECTORIES "/usr/include"
)
add_executable(HelcinyAlsaWorker src/alsa_worker.c)
set_target_properties(HelcinyAlsaWorker PROPERTIES OUTPUT_NAME "HelcinyAlsaWorker.bin" SUFFIX "")
target_link_libraries(HelcinyAlsaWorker PRIVATE ALSA::ALSA MPG123::MPG123 m)

add_executable(AlsaWorkerTest src/alsa_worker_launcher.cpp)
set_target_properties(AlsaWorkerTest PROPERTIES OUTPUT_NAME "AlsaWorkerTest.app" SUFFIX "")
target_link_libraries(AlsaWorkerTest PRIVATE PocketBook::InkView)
target_include_directories(AlsaWorkerTest PRIVATE "${POCKETBOOK_SDK_INCLUDE_DIR}" "/usr/include/freetype2")
