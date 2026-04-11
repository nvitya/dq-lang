cmake_minimum_required(VERSION 3.10)
project(DqComp VERSION 0.1 LANGUAGES CXX)

set(DQ_BUILD_DIR "${CMAKE_SOURCE_DIR}/build")

# Keep the user-visible binaries in ./build even for in-source builds.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${DQ_BUILD_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${DQ_BUILD_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${DQ_BUILD_DIR}")

if(CMAKE_CONFIGURATION_TYPES)
  foreach(cfg ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${cfg}" cfg_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg_upper} "${DQ_BUILD_DIR}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${cfg_upper} "${DQ_BUILD_DIR}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${cfg_upper} "${DQ_BUILD_DIR}")
  endforeach()
endif()

# On newer CMake versions this makes "make test" build the default targets first.
set(CMAKE_SKIP_TEST_ALL_DEPENDENCY FALSE)

add_subdirectory(compiler build/compiler)
add_subdirectory(autotest/atrunner build/autotest/atrunner)

include(CTest)

if(BUILD_TESTING)
  add_test(
    NAME compiler-autotest
    COMMAND "${CMAKE_SOURCE_DIR}/autotest/run-all.sh"
  )
  set_tests_properties(compiler-autotest PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )
endif()

add_custom_target(check
  COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure
  DEPENDS dq-comp dqatrun
  USES_TERMINAL
  COMMENT "Running compiler autotests"
)

add_custom_target(comptest
  COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure
  DEPENDS dq-comp dqatrun
  USES_TERMINAL
  COMMENT "Running compiler autotests"
)
