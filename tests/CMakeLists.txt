set(CMAKE_SKIP_RPATH ON)

add_library(test_base SHARED base.c)

add_library(test_lib SHARED lib.c)
target_link_libraries(
  test_lib
  PRIVATE
  test_base
  "-Wl,-R,'$$ORIGIN'"
  )

add_executable(test_exe exe.c)
target_link_libraries(
  test_exe
  test_lib
  "-Wl,-R,'$$ORIGIN'"
  )
