set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(DQ_MINGW_ROOT "" CACHE PATH "Root of an llvm-mingw or MinGW-w64 cross toolchain")

if(DQ_MINGW_ROOT)
  set(CMAKE_C_COMPILER "${DQ_MINGW_ROOT}/bin/x86_64-w64-mingw32-clang")
  set(CMAKE_CXX_COMPILER "${DQ_MINGW_ROOT}/bin/x86_64-w64-mingw32-clang++")

  # llvm-mingw keeps target headers/libs under the target triple directory, while
  # host tools such as clang/lld live directly under the toolchain root.
  set(CMAKE_FIND_ROOT_PATH
    "${DQ_MINGW_ROOT}/x86_64-w64-mingw32"
    "${DQ_MINGW_ROOT}"
  )
else()
  set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
  set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)

  # Where is the target environment
  set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
endif()

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# For libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
