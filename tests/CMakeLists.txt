# SPDX-License-Identifier: 0BSD

CPMAddPackage("gh:krxecs/Munit.cmake@0.1.0")
munit_cmake_init()

include(DownloadMunitCPM)

if (UNIX)
  add_executable(explicit-memset_tests explicit-memset.test.c)
  target_link_libraries(explicit-memset_tests explicit-memset munit)

  munit_discover_tests(explicit-memset_tests)
endif ()
