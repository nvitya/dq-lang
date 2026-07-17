cmake_minimum_required(VERSION 3.10)
project(DqComp VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CheckCXXSourceCompiles)

find_library(DQ_STDCXXEXP_LIBRARY stdc++exp)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND DQ_STDCXXEXP_LIBRARY)
  set(CMAKE_REQUIRED_LIBRARIES "${DQ_STDCXXEXP_LIBRARY}")
endif()
unset(DQ_HAVE_STD_PRINT CACHE)
check_cxx_source_compiles([=[
  #include <print>
  int main()
  {
    std::print("ok");
    return 0;
  }
]=] DQ_HAVE_STD_PRINT)
unset(CMAKE_REQUIRED_LIBRARIES)

if(NOT DQ_HAVE_STD_PRINT)
  message(FATAL_ERROR
    "DQ requires C++23 std::print support. "
    "On Ubuntu 24.04, the default GCC 13/libstdc++ does not provide <print>; "
    "install GCC 14 or newer and reconfigure, for example with "
    "CC=gcc-14 CXX=g++-14 cmake .")
endif()

if(CMAKE_CROSSCOMPILING)
  # For cross-builds, place binaries in a bin/ folder within the cross build directory
  set(DQ_BUILD_DIR "${CMAKE_BINARY_DIR}/bin")
else()
  # Keep the user-visible binaries in ./build even for in-source builds.
  set(DQ_BUILD_DIR "${CMAKE_SOURCE_DIR}/build")
endif()

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

install(DIRECTORY "${CMAKE_SOURCE_DIR}/stdpkg/"
  DESTINATION lib/dq/stdpkg
  FILES_MATCHING
    PATTERN "*.dq"
    PATTERN "*.dqh"
    PATTERN "*.dqi"
    PATTERN ".dqbuild" EXCLUDE
)

set(DQ_AUTOTEST_COMMAND "$<TARGET_FILE:dqatrun>" -c "$<TARGET_FILE:dq-comp>" -r autotest/tests)

add_custom_target(cleantest
  COMMAND "${CMAKE_SOURCE_DIR}/autotest/clean.sh"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  USES_TERMINAL
)

add_custom_target(autotest
  COMMAND ${DQ_AUTOTEST_COMMAND}
  DEPENDS dq-comp dqatrun
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  USES_TERMINAL
)

add_custom_target(autotest_O0
  COMMAND ${DQ_AUTOTEST_COMMAND} -O0
  DEPENDS dq-comp dqatrun
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  USES_TERMINAL
)

add_custom_target(test
  COMMAND "${CMAKE_SOURCE_DIR}/autotest/clean.sh"
  COMMAND ${DQ_AUTOTEST_COMMAND}
  DEPENDS dq-comp dqatrun
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  USES_TERMINAL
)

add_custom_target(test_O0
  COMMAND "${CMAKE_SOURCE_DIR}/autotest/clean.sh"
  COMMAND ${DQ_AUTOTEST_COMMAND} -O0
  DEPENDS dq-comp dqatrun
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  USES_TERMINAL
)

add_custom_target(check
  DEPENDS autotest
)

add_custom_target(comptest
  DEPENDS autotest
)
