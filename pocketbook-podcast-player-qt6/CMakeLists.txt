cmake_minimum_required(VERSION 3.21)
project(helciny-podcasty-qt6 CXX)

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

