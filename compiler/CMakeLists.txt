cmake_minimum_required(VERSION 3.10)
project(GlobProject VERSION 1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find LLVM
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

find_program(DQ_DEFAULT_LINK_DRIVER
  NAMES "clang++-${LLVM_VERSION_MAJOR}" clang++
  HINTS "${LLVM_TOOLS_BINARY_DIR}"
)
if(DQ_DEFAULT_LINK_DRIVER)
  message(STATUS "Using DQ default link driver: ${DQ_DEFAULT_LINK_DRIVER}")
else()
  message(WARNING "Could not find clang++; dq-comp will require DQ_LINKER_DRIVER at runtime")
endif()

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# Enable Debug symbols
add_compile_options(-g)

include_directories(src)
include_directories(ast)
include_directories(types)
include_directories(parser)
include_directories(builtins)
include_directories(codegen)
include_directories(src_test)
include_directories(utils)
include_directories(tools)

file(GLOB SRC_SRC       CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB SRC_AST       CONFIGURE_DEPENDS "ast/*.cpp")
file(GLOB SRC_TYPES     CONFIGURE_DEPENDS "types/*.cpp")
file(GLOB SRC_CODEGEN   CONFIGURE_DEPENDS "codegen/*.cpp")
file(GLOB SRC_PARSER    CONFIGURE_DEPENDS "parser/*.cpp")
file(GLOB SRC_BUILTINS  CONFIGURE_DEPENDS "builtins/*.cpp")
file(GLOB SRC_SRC_TEST  CONFIGURE_DEPENDS "src_test/*.cpp")
file(GLOB SRC_UTILS     CONFIGURE_DEPENDS "utils/*.cpp")

add_executable(dq-comp ${SRC_SRC} ${SRC_AST} ${SRC_TYPES} ${SRC_PARSER} ${SRC_CODEGEN} ${SRC_BUILTINS} ${SRC_SRC_TEST} ${SRC_UTILS})
add_executable(dq-run tools/main_dq_run.cpp ${SRC_UTILS})
add_executable(dq-inline-asm-test
    test/inline_asm_target_test.cpp
    codegen/inline_asm_target.cpp
    codegen/inline_asm_target_x86.cpp
    codegen/inline_asm_target_arm.cpp
    codegen/inline_asm_target_aarch64.cpp
    codegen/inline_asm_target_riscv.cpp)
add_executable(dq-projectfile-test
    test/projectfile_test.cpp
    src/projectfile.cpp
    src/comp_options.cpp
    ast/module_path.cpp
    utils/strparse.cpp
    utils/executable_path.cpp)

set(dq_llvm_targets native)
set(dq_llvm_definitions)
foreach(target ARM RISCV WebAssembly)
  if(target IN_LIST LLVM_TARGETS_TO_BUILD)
    list(APPEND dq_llvm_targets ${target})
    string(TOUPPER ${target} target_upper)
    list(APPEND dq_llvm_definitions DQ_LLVM_HAS_${target_upper})
  endif()
endforeach()
target_compile_definitions(dq-comp PRIVATE ${dq_llvm_definitions})
target_compile_definitions(dq-projectfile-test PRIVATE ${dq_llvm_definitions})

if(DQ_DEFAULT_LINK_DRIVER)
  target_compile_definitions(dq-comp PRIVATE DQ_DEFAULT_LINK_DRIVER="${DQ_DEFAULT_LINK_DRIVER}")
endif()
target_compile_definitions(dq-comp PRIVATE DQ_DEFAULT_GCCLIBS_DIR="${CMAKE_SOURCE_DIR}/gcclibs")

# on exception I want to have a nice backtrace
set_target_properties(dq-comp PROPERTIES ENABLE_EXPORTS ON)

# Link LLVM libraries
llvm_map_components_to_libnames(llvm_libs
    core
    support
    ${dq_llvm_targets}
    irreader
    bitwriter
    passes
)

find_library(DQ_STDCXXEXP_LIBRARY stdc++exp)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_libraries(dq-comp PRIVATE stdc++exp)
  target_link_libraries(dq-run PRIVATE stdc++exp)
  target_link_libraries(dq-inline-asm-test PRIVATE stdc++exp)
  target_link_libraries(dq-projectfile-test PRIVATE stdc++exp)
elseif(DQ_STDCXXEXP_LIBRARY)
  target_link_libraries(dq-comp PRIVATE ${DQ_STDCXXEXP_LIBRARY})
  target_link_libraries(dq-run PRIVATE ${DQ_STDCXXEXP_LIBRARY})
  target_link_libraries(dq-inline-asm-test PRIVATE ${DQ_STDCXXEXP_LIBRARY})
  target_link_libraries(dq-projectfile-test PRIVATE ${DQ_STDCXXEXP_LIBRARY})
endif()

target_link_libraries(dq-comp PRIVATE ${llvm_libs})

install(TARGETS dq-comp dq-run DESTINATION bin)
