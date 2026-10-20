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

  # --- R03 transfer protocol (BLE app contract) ---
  add_executable(test_r03
      test_r03.c
      ${FW_ROOT}/components/r03_transfer/r03.c)
  target_include_directories(test_r03 PRIVATE
      ${FW_ROOT}/components/r03_transfer/include)
  add_test(NAME r03 COMMAND test_r03)

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
      target_compile_options(test_r03 PRIVATE -Wall -Wextra)
      target_compile_options(caipo_protocol_host_test PRIVATE -Wall -Wextra)
  endif()
