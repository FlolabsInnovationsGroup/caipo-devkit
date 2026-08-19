<<<<<<< HEAD
# Host-side tests: plain CMake, no ESP-IDF. Builds hardware-independent logic
# (gesture classifier, protocol) natively so it can run on the dev machine.
#
#   cmake -S . -B build && cmake --build build && ctest --test-dir build

cmake_minimum_required(VERSION 3.16)
project(caipo_host_tests C)

set(CMAKE_C_STANDARD 11)
set(FW_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

enable_testing()

add_executable(test_touch_gesture
    test_touch_gesture.c
    ${FW_ROOT}/components/touch_ttp223/touch_gesture.c)
target_include_directories(test_touch_gesture PRIVATE
    ${FW_ROOT}/components/touch_ttp223/include)
add_test(NAME touch_gesture COMMAND test_touch_gesture)

# test_protocol_host.c joins here once caipo_protocol is implemented.
=======
cmake_minimum_required(VERSION 3.16)
project(caipo_protocol_host_test C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(caipo_protocol_host_test
    test_protocol_host.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/caipo_protocol/caipo_protocol.c
)

target_include_directories(caipo_protocol_host_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/caipo_protocol/include
)

get_filename_component(CAIPO_VECTORS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../protocol/vectors" ABSOLUTE)
target_compile_definitions(caipo_protocol_host_test PRIVATE
    CAIPO_VECTORS_DIR="${CAIPO_VECTORS_DIR}"
)

if(NOT MSVC)
    target_compile_options(caipo_protocol_host_test PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_test(NAME caipo_protocol_host_test COMMAND caipo_protocol_host_test)
>>>>>>> origin/Sabry-Raspberry_Pi
