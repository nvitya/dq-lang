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

file(GLOB SRC_SRC       "src/*.cpp")
file(GLOB SRC_AST       "ast/*.cpp")
file(GLOB SRC_TYPES     "types/*.cpp")
file(GLOB SRC_CODEGEN   "codegen/*.cpp")
file(GLOB SRC_PARSER    "parser/*.cpp")
file(GLOB SRC_BUILTINS  "builtins/*.cpp")
file(GLOB SRC_SRC_TEST  "src_test/*.cpp")
file(GLOB SRC_UTILS     "utils/*.cpp")


add_executable(dq-comp ${SRC_SRC} ${SRC_AST} ${SRC_TYPES} ${SRC_PARSER} ${SRC_CODEGEN} ${SRC_BUILTINS} ${SRC_SRC_TEST} ${SRC_UTILS})

# on exception I want to have a nice backtrace
set_target_properties(dq-comp PROPERTIES ENABLE_EXPORTS ON)

# Link LLVM libraries
llvm_map_components_to_libnames(llvm_libs
    core
    support
    native
    irreader
    passes
)

#target_link_libraries(dqc ${llvm_libs})

target_link_libraries(dq-comp PRIVATE stdc++exp ${llvm_libs})

# for early compiler development:
add_custom_command(TARGET dq-comp POST_BUILD
    COMMAND cp $<TARGET_FILE:dq-comp> /usr/local/bin/dq-comp
    COMMENT "Copying dq-comp to /usr/local/bin..."
)

# Define the installation destination
install(TARGETS dq-comp DESTINATION /usr/local/bin)
