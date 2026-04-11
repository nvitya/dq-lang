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

add_subdirectory(compiler build/compiler)
add_subdirectory(autotest/atrunner build/autotest/atrunner)

add_custom_target(autotest
  COMMAND "$<TARGET_FILE:dqatrun>" -c build/dq-comp -r autotest/tests
  DEPENDS dq-comp dqatrun
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  USES_TERMINAL
)

add_custom_target(test
  DEPENDS autotest
)

add_custom_target(check
  DEPENDS autotest
)

add_custom_target(comptest
  DEPENDS autotest
)
