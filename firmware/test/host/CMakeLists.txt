cmake_minimum_required(VERSION 3.16)
  project(caipo_host_tests C)

  set(CMAKE_C_STANDARD 11)
  set(CMAKE_C_STANDARD_REQUIRED ON)
  set(FW_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

  enable_testing()

  # --- touch gesture classifier ---
  add_executable(test_touch_gesture
      test_touch_gesture.c
      ${FW_ROOT}/components/touch_ttp223/touch_gesture.c)
  target_include_directories(test_touch_gesture PRIVATE
      ${FW_ROOT}/components/touch_ttp223/include)
  add_test(NAME touch_gesture COMMAND test_touch_gesture)

  # --- protocol codec ---
  add_executable(caipo_protocol_host_test
      test_protocol_host.c
      ${FW_ROOT}/components/caipo_protocol/caipo_protocol.c)
  target_include_directories(caipo_protocol_host_test PRIVATE
      ${FW_ROOT}/components/caipo_protocol/include)
  get_filename_component(CAIPO_VECTORS_DIR "${FW_ROOT}/../protocol/vectors" ABSOLUTE)
  target_compile_definitions(caipo_protocol_host_test PRIVATE
      CAIPO_VECTORS_DIR="${CAIPO_VECTORS_DIR}")
  add_test(NAME caipo_protocol_host_test COMMAND caipo_protocol_host_test)

  if(NOT MSVC)
      target_compile_options(test_touch_gesture PRIVATE -Wall -Wextra)
      target_compile_options(caipo_protocol_host_test PRIVATE -Wall -Wextra)
  endif()