cmake_minimum_required(VERSION 3.22.1)
project(replicant_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(oboe REQUIRED CONFIG)

add_library(replicant_engine SHARED
    jni_bridge.cpp
    engine/firmware.cpp
    engine/acb_voice.cpp
    engine/master_bus.cpp
    engine/engine.cpp
    engine/audio_driver.cpp
)

target_include_directories(replicant_engine PRIVATE engine)

target_compile_options(replicant_engine PRIVATE
    -O3
    -ffast-math
    -Wall
    -Wextra
    -Wno-unused-parameter
)

target_link_libraries(replicant_engine PRIVATE
    oboe::oboe
    android
    log
)
