cmake_minimum_required(VERSION 3.10)
project(GlobProject VERSION 1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find LLVM
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

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

# on exception I want to have a nice backtrace
set_target_properties(dq-comp PROPERTIES ENABLE_EXPORTS ON)

# Link LLVM libraries
llvm_map_components_to_libnames(llvm_libs
    core
    support
    object
    native
    irreader
    passes
)

find_library(DQ_STDCXXEXP_LIBRARY stdc++exp)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_libraries(dq-comp PRIVATE stdc++exp)
  target_link_libraries(dq-run PRIVATE stdc++exp)
elseif(DQ_STDCXXEXP_LIBRARY)
  target_link_libraries(dq-comp PRIVATE ${DQ_STDCXXEXP_LIBRARY})
  target_link_libraries(dq-run PRIVATE ${DQ_STDCXXEXP_LIBRARY})
endif()

target_link_libraries(dq-comp PRIVATE ${llvm_libs})

install(TARGETS dq-comp dq-run DESTINATION bin)
