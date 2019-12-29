cmake_minimum_required(VERSION 2.8.12)
project(sold)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

add_executable(
  sold
  sold.cc
  )