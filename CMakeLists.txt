# SPDX-License-Identifier: 0BSD

cmake_policy(SET CMP0091 NEW)

cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(explicit-memset
  LANGUAGES C
  VERSION 0.1.0
)

include(cmake/DefaultConfig.cmake)

####################
# Section: Options #
####################

include(CMakeDependentOption)
include(CTest)

cmake_dependent_option(${PROJECT_NAME}_BUILD_TESTING
  "Force building tests for ${PROJECT_NAME}."
  ON
  "BUILD_TESTING;CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME"
  OFF
)

cmake_dependent_option(${PROJECT_NAME}_INSTALL
  "Install all targets of ${PROJECT_NAME}."
  ON
  "CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME"
  OFF
)

# Add CPM (https://github.com/cpm-cmake/CPM.cmake)
include(cmake/CPM.cmake)

####################
# Section: Targets #
####################

set(ALL_INCLUDES
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>  # <prefix>/include/mylib
)

add_library(explicit-memset src/ay/explicit-memset.c)
target_sources(explicit-memset INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/ay>
)
target_include_directories(explicit-memset
  PRIVATE "${ALL_INCLUDES}"
  SYSTEM INTERFACE "${ALL_INCLUDES}"
)
add_library(${PROJECT_NAME}::explicit-memset ALIAS explicit-memset)

#######################
# Section: Installing #
#######################

if (${PROJECT_NAME}_INSTALL)
  install(
    TARGETS explicit-memset
    EXPORT explicit-memset-targets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  )

  include(CMakePackageConfigHelpers)
  write_basic_package_version_file("${PROJECT_NAME}ConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
  )

  configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}Config.cmake.in"
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake"
  )

  install(
    EXPORT explicit-memset-targets
    FILE "${PROJECT_NAME}-targets.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    NAMESPACE ${PROJECT_NAME}::
  )

  install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/ay"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  )

  install(
    FILES
      "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  )
endif ()

####################
# Section: Testing #
####################

if (${PROJECT_NAME}_BUILD_TESTING)
  add_subdirectory(tests)
endif ()

