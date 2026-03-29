cmake_minimum_required(VERSION 3.10)
project(DqAtrunner VERSION 0.1)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Debug symbols
add_compile_options(-g)

include_directories(.)
include_directories(../../compiler/utils)

file(GLOB SRC_ATRUNNER CONFIGURE_DEPENDS "*.cpp")

add_executable(dqatrun ${SRC_ATRUNNER} ../../compiler/utils/processrunner.cpp)

# on exception I want to have a nice backtrace
set_target_properties(dqatrun PROPERTIES ENABLE_EXPORTS ON)

target_link_libraries(dqatrun PRIVATE stdc++exp)

install(TARGETS dqatrun DESTINATION /usr/local/bin)
