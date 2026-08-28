cmake_minimum_required(VERSION 3.10)
project(DqComp VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CheckCXXSourceCompiles)
include(GNUInstallDirs)

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

install(DIRECTORY "${CMAKE_SOURCE_DIR}/gcclibs/"
  DESTINATION lib/dq/gcclibs
)

set(DQ_GDB_SYSTEM_INIT_DIR "/etc/gdb/gdbinit.d" CACHE PATH
  "GDB system-wide initialization directory; empty disables auto-loading"
)
set(DQ_GDB_PRINTERS_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/dq/gdb")
set(DQ_GDB_PRINTERS_FILE
  "${CMAKE_INSTALL_FULL_DATADIR}/dq/gdb/dq_printers.py"
)

configure_file(
  "${CMAKE_SOURCE_DIR}/tools/gdb/dq_gdbinit.py"
  "${DQ_BUILD_DIR}/dq_gdbinit.py"
  @ONLY
)

install(FILES "${CMAKE_SOURCE_DIR}/tools/gdb/dq_printers.py"
  DESTINATION "${DQ_GDB_PRINTERS_INSTALL_DIR}"
)

if(DQ_GDB_SYSTEM_INIT_DIR)
  install(FILES "${DQ_BUILD_DIR}/dq_gdbinit.py"
    DESTINATION "${DQ_GDB_SYSTEM_INIT_DIR}"
  )
endif()

set(DQ_AUTOTEST_COMMAND "$<TARGET_FILE:dqatrun>" -c "$<TARGET_FILE:dq-comp>" -r autotest/tests)

set(DQ_ARM_BARE_COMPILE_TARGETS
  arm_m0-bare
  arm_m3-bare
  arm_m4-bare
  arm_m4f-bare
  arm_m33f-bare
  arm_m7fd-bare
)
set(DQ_ARM_TARGET_TEST_COMMANDS)
foreach(DQ_ARM_TARGET IN LISTS DQ_ARM_BARE_COMPILE_TARGETS)
  list(APPEND DQ_ARM_TARGET_TEST_COMMANDS
    COMMAND "$<TARGET_FILE:dq-comp>" "--target=${DQ_ARM_TARGET}" --no-use-sys -c
            -o "${DQ_BUILD_DIR}/${DQ_ARM_TARGET}_pointer_const.o"
            "${CMAKE_SOURCE_DIR}/autotest/target/arm_m7f_pointer_const.dq"
  )
endforeach()

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
  COMMAND python3 "${CMAKE_SOURCE_DIR}/tools/test_dq_v1_to_v2.py"
  COMMAND "$<TARGET_FILE:dq-inline-asm-test>"
  COMMAND "$<TARGET_FILE:dq-projectfile-test>"
  ${DQ_ARM_TARGET_TEST_COMMANDS}
  COMMAND "$<TARGET_FILE:dq-comp>" --target=arm_m7f-bare --no-use-sys -c
          -o "${DQ_BUILD_DIR}/arm_m7f_inline_asm.o"
          "${CMAKE_SOURCE_DIR}/autotest/target/arm_m7f_inline_asm.dq"
  COMMAND "$<TARGET_FILE:dq-comp>" --target=arm_m7f-bare --no-use-sys -c
          -o "${DQ_BUILD_DIR}/arm_m7f_pointer_const.o"
          "${CMAKE_SOURCE_DIR}/autotest/target/arm_m7f_pointer_const.dq"
  COMMAND "$<TARGET_FILE:dq-comp>" --target=arm_m7f-bare --no-use-sys -c
          -o "${DQ_BUILD_DIR}/arm_m7f_float_default.o"
          "${CMAKE_SOURCE_DIR}/autotest/target/arm_float_default.dq"
  COMMAND "$<TARGET_FILE:dq-comp>" --target=arm_m7fd-bare --no-use-sys -c
          -o "${DQ_BUILD_DIR}/arm_m7fd_float_default.o"
          "${CMAKE_SOURCE_DIR}/autotest/target/arm_float_default.dq"
  COMMAND "$<TARGET_FILE:dq-comp>" --target=arm_m7f-bare --dynstrings
          --build-root "${DQ_BUILD_DIR}/arm_m7f_native_collections_build" -c
          -o "${DQ_BUILD_DIR}/arm_m7f_native_collections.o"
          "${CMAKE_SOURCE_DIR}/autotest/target/arm_native_collections.dq"
  COMMAND "$<TARGET_FILE:dq-comp>" -c -DEXPECT_EXCEPTIONS
          -o "${DQ_BUILD_DIR}/host_exceptions_default.o"
          "${CMAKE_SOURCE_DIR}/autotest/target/exception_switch.dq"
  COMMAND "$<TARGET_FILE:dq-comp>" --no-exceptions -c
          -o "${DQ_BUILD_DIR}/host_exceptions_disabled.o"
          "${CMAKE_SOURCE_DIR}/autotest/target/exception_switch.dq"
  COMMAND "$<TARGET_FILE:dq-comp>" --target=arm_m7f-bare -c
          -o "${DQ_BUILD_DIR}/arm_exceptions_default.o"
          "${CMAKE_SOURCE_DIR}/autotest/target/exception_switch.dq"
  COMMAND "$<TARGET_FILE:dq-comp>" --target=arm_m7f-bare --exceptions -DEXPECT_EXCEPTIONS -c
          -o "${DQ_BUILD_DIR}/arm_exceptions_enabled.o"
          "${CMAKE_SOURCE_DIR}/autotest/target/exception_switch.dq"
  COMMAND "$<TARGET_FILE:dq-comp>" --no-exceptions --exceptions -DEXPECT_EXCEPTIONS -c
          -o "${DQ_BUILD_DIR}/exceptions_last_enabled.o"
          "${CMAKE_SOURCE_DIR}/autotest/target/exception_switch.dq"
  COMMAND "$<TARGET_FILE:dq-comp>" --exceptions --no-exceptions -c
          -o "${DQ_BUILD_DIR}/exceptions_last_disabled.o"
          "${CMAKE_SOURCE_DIR}/autotest/target/exception_switch.dq"
  COMMAND "$<TARGET_FILE:dq-comp>" --exceptions -DEXPECT_EXCEPTIONS -c
          -o "${DQ_BUILD_DIR}/project_exceptions_override.o"
          "${CMAKE_SOURCE_DIR}/autotest/tests/project/exceptions_false.dqproj"
  COMMAND "$<TARGET_FILE:dq-comp>" -c -DEXPECT_DYNSTRINGS
          -o "${DQ_BUILD_DIR}/host_dynstrings_default.o"
          "${CMAKE_SOURCE_DIR}/autotest/target/dynstrings_switch.dq"
  COMMAND "$<TARGET_FILE:dq-comp>" --no-dynstrings -c
          -o "${DQ_BUILD_DIR}/host_dynstrings_disabled.o"
          "${CMAKE_SOURCE_DIR}/autotest/target/dynstrings_switch.dq"
  COMMAND "$<TARGET_FILE:dq-comp>" --target=arm_m7f-bare -c
          -o "${DQ_BUILD_DIR}/arm_dynstrings_default.o"
          "${CMAKE_SOURCE_DIR}/autotest/target/dynstrings_switch.dq"
  COMMAND "$<TARGET_FILE:dq-comp>" --target=arm_m7f-bare --dynstrings -DEXPECT_DYNSTRINGS -c
          -o "${DQ_BUILD_DIR}/arm_dynstrings_enabled.o"
          "${CMAKE_SOURCE_DIR}/autotest/target/dynstrings_switch.dq"
  COMMAND "$<TARGET_FILE:dq-comp>" --no-dynstrings --dynstrings -DEXPECT_DYNSTRINGS -c
          -o "${DQ_BUILD_DIR}/dynstrings_last_enabled.o"
          "${CMAKE_SOURCE_DIR}/autotest/target/dynstrings_switch.dq"
  COMMAND "$<TARGET_FILE:dq-comp>" --dynstrings --no-dynstrings -c
          -o "${DQ_BUILD_DIR}/dynstrings_last_disabled.o"
          "${CMAKE_SOURCE_DIR}/autotest/target/dynstrings_switch.dq"
  COMMAND "$<TARGET_FILE:dq-comp>" --dynstrings -DEXPECT_DYNSTRINGS -c
          -o "${DQ_BUILD_DIR}/project_dynstrings_override.o"
          "${CMAKE_SOURCE_DIR}/autotest/tests/project/dynstrings_false.dqproj"
  COMMAND "$<TARGET_FILE:dq-comp>" --target=arm_m7f-bare --no-dynstrings -c
          -o "${DQ_BUILD_DIR}/arm_no_dynstrings_text.o"
          "${CMAKE_SOURCE_DIR}/autotest/target/no_dynstrings_text.dq"
  COMMAND "${CMAKE_COMMAND}"
          "-DDQ_NM=${CMAKE_NM}"
          "-DDQ_OBJECT=${DQ_BUILD_DIR}/arm_no_dynstrings_text.o"
          -P "${CMAKE_SOURCE_DIR}/compiler/test/no_dynstrings_symbols_test.cmake"
  COMMAND "${CMAKE_COMMAND}"
          "-DDQ_COMPILER=$<TARGET_FILE:dq-comp>"
          "-DDQ_SOURCE=${CMAKE_SOURCE_DIR}/autotest/target/dynstrings_disabled_error.dq"
          "-DDQ_OUTPUT=${DQ_BUILD_DIR}/dynstrings_disabled_error.o"
          -P "${CMAKE_SOURCE_DIR}/compiler/test/no_dynstrings_test.cmake"
  COMMAND "$<TARGET_FILE:dq-comp>" -o "${DQ_BUILD_DIR}/arm_no_exceptions.elf"
          "${CMAKE_SOURCE_DIR}/autotest/tests/project/arm_no_exceptions.dqproj"
  COMMAND "${CMAKE_COMMAND}"
          "-DDQ_NM=${CMAKE_NM}"
          "-DDQ_BINARY=${DQ_BUILD_DIR}/arm_no_exceptions.elf"
          -P "${CMAKE_SOURCE_DIR}/compiler/test/no_unwind_test.cmake"
  COMMAND "${CMAKE_COMMAND}"
          "-DDQ_COMPILER=$<TARGET_FILE:dq-comp>"
          "-DDQ_SOURCE=${CMAKE_SOURCE_DIR}/autotest/target/exceptions_disabled_error.dq"
          "-DDQ_SYS_SOURCE=${CMAKE_SOURCE_DIR}/autotest/target/exception_switch.dq"
          "-DDQ_OUTPUT=${DQ_BUILD_DIR}/exceptions_disabled_error.o"
          -P "${CMAKE_SOURCE_DIR}/compiler/test/no_exceptions_test.cmake"
  COMMAND "$<TARGET_FILE:dq-comp>" --no-exceptions
          -o "${DQ_BUILD_DIR}/no_exceptions_fallback"
          "${CMAKE_SOURCE_DIR}/autotest/target/no_exceptions_fallback.dq"
  COMMAND "${DQ_BUILD_DIR}/no_exceptions_fallback"
  COMMAND "$<TARGET_FILE:dq-comp>" --target=arm_m7f-bare --no-use-sys --link
          --linker-arg=--entry=_start
          -o "${DQ_BUILD_DIR}/arm_m7f_module_init.elf"
          "${CMAKE_SOURCE_DIR}/autotest/target/arm_m7f_module_init.dq"
  COMMAND "$<TARGET_FILE:dq-comp>" --target=arm_m7f-bare --no-use-sys --link --lto -O1
          --linker-arg=--entry=soft_entry
          --linker-arg=--defsym=end=0
          -o "${DQ_BUILD_DIR}/arm_m7f_lto_asm_export.elf"
          "${CMAKE_SOURCE_DIR}/autotest/target/arm_m7f_lto_asm_export.dq"
  COMMAND "$<TARGET_FILE:dq-comp>" --target=arm_m0-bare --no-use-sys --link
          --linker-arg=--entry=_start
          --linker-arg=--undefined=memset
          --linker-arg=--undefined=__aeabi_uidiv
          -o "${DQ_BUILD_DIR}/arm_m0_bundled_runtimes.elf"
          "${CMAKE_SOURCE_DIR}/autotest/target/arm_m7f_module_init.dq"
  COMMAND "$<TARGET_FILE:dq-comp>" -o "${DQ_BUILD_DIR}/arm_m7f_project.o"
          "${CMAKE_SOURCE_DIR}/autotest/tests/project/arm_m7f.dqproj"
  COMMAND "$<TARGET_FILE:dq-comp>" -O0 -g -DPROJECT_OVERRIDE=2 -DEXPECT_OVERRIDE
          -o "${DQ_BUILD_DIR}/project_override"
          "${CMAKE_SOURCE_DIR}/autotest/tests/project/basic.dqproj"
  COMMAND "${DQ_BUILD_DIR}/project_override"
  COMMAND "$<TARGET_FILE:dq-comp>" --link -o "${DQ_BUILD_DIR}/project_linkmode"
          "${CMAKE_SOURCE_DIR}/autotest/tests/project/linkmode_false.dqproj"
  COMMAND "${DQ_BUILD_DIR}/project_linkmode"
  COMMAND "${CMAKE_CXX_COMPILER}" -c "${CMAKE_SOURCE_DIR}/compiler/test/project_linkobject.cpp"
          -o "${DQ_BUILD_DIR}/project_linkobject.o"
  COMMAND "$<TARGET_FILE:dq-comp>" --linker-arg=--no-undefined
          -o "${DQ_BUILD_DIR}/project_linkobject"
          "${CMAKE_SOURCE_DIR}/autotest/tests/project/linkobject.dqproj"
  COMMAND "${DQ_BUILD_DIR}/project_linkobject"
  COMMAND "$<TARGET_FILE:dq-comp>" -c -o "${DQ_BUILD_DIR}/project_compile_only.o"
          "${CMAKE_SOURCE_DIR}/autotest/tests/project/linkobject.dqproj"
  COMMAND ${DQ_AUTOTEST_COMMAND}
  DEPENDS dq-comp dqatrun dq-inline-asm-test dq-projectfile-test
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
