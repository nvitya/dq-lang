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

find_library(DQ_STDCXXEXP_LIBRARY stdc++exp)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_libraries(dqatrun PRIVATE stdc++exp)
elseif(DQ_STDCXXEXP_LIBRARY)
  target_link_libraries(dqatrun PRIVATE ${DQ_STDCXXEXP_LIBRARY})
endif()

install(TARGETS dqatrun DESTINATION bin)
