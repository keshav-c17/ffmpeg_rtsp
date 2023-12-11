cmake_minimum_required(VERSION 3.16)

project(rtsp_ffmpeg)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
    libavformat
    libavcodec
    libavutil
)

list(APPEND SOURCES
    src/main.cpp
    src/transcoder.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
    PkgConfig::LIBAV
    stdc++fs       # <filesystem> library for calculating filesize.
)

# Set the output directory to the current source directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)