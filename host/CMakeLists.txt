cmake_minimum_required(VERSION 3.22)
project(rt8_host LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ENGINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../app/src/main/cpp/engine)

add_library(rt8_engine STATIC
    ${ENGINE_DIR}/firmware.cpp
    ${ENGINE_DIR}/acb_voice.cpp
    ${ENGINE_DIR}/master_bus.cpp
    ${ENGINE_DIR}/engine.cpp
)
target_include_directories(rt8_engine PUBLIC ${ENGINE_DIR})
target_compile_options(rt8_engine PRIVATE -O2 -fno-math-errno -fno-trapping-math -Wall -Wextra -Wno-unused-parameter)
target_compile_definitions(rt8_engine PUBLIC RT8_HOST_TESTS)

add_executable(engine_tests engine_tests.cpp)
target_link_libraries(engine_tests PRIVATE rt8_engine)

add_executable(render_demo render_demo.cpp)
target_link_libraries(render_demo PRIVATE rt8_engine)

add_executable(firmware_test firmware_test.cpp)
target_link_libraries(firmware_test PRIVATE rt8_engine)

enable_testing()
add_test(NAME engine_tests COMMAND engine_tests)
add_test(NAME firmware_test COMMAND firmware_test ${CMAKE_CURRENT_SOURCE_DIR}/../.firmware/TR8S_UP.bin)
