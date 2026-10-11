#!/bin/bash
# Source this script to configure Windows cross-compilation and Wine.
export MESA_VK_IGNORE_CONFORMANCE_WARNING=1
export DQ_MINGW_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/sysroots/llvm-mingw-x86_64"
export WINEPATH="$(winepath -w "$DQ_MINGW_ROOT/x86_64-w64-mingw32/bin")"
