cmake_minimum_required(VERSION 2.8.12)
project(sold LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

add_executable(
  sold
  sold.cc
  )

add_subdirectory(tests)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/test_out
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/sold ${CMAKE_CURRENT_BINARY_DIR}/tests/test_exe ${CMAKE_CURRENT_BINARY_DIR}/tests/test_out
  MAIN_DEPENDENCY sold
  DEPENDS test_exe
  )
add_custom_target(test_link ALL DEPENDS tests/test_out)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/test_out_stamp
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tests/test_out
  DEPENDS test_link
  )
add_custom_target(run_test_out ALL DEPENDS tests/test_out_stamp)
