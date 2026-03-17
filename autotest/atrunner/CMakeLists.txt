cmake_minimum_required(VERSION 3.10)
project(DqAtrunner VERSION 0.1)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Debug symbols
add_compile_options(-g)

include_directories(.)

file(GLOB SRC_ATRUNNER CONFIGURE_DEPENDS "*.cpp")

add_executable(dqatrun ${SRC_ATRUNNER})

# on exception I want to have a nice backtrace
set_target_properties(dqatrun PROPERTIES ENABLE_EXPORTS ON)

target_link_libraries(dqatrun PRIVATE stdc++exp)

# for early runner development:
add_custom_command(TARGET dqatrun POST_BUILD
    COMMAND cp $<TARGET_FILE:dqatrun> /usr/local/bin/dqatrun
    COMMENT "Copying dqatrun to /usr/local/bin..."
)

install(TARGETS dqatrun DESTINATION /usr/local/bin)

