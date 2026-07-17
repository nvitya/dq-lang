#!/bin/sh

set -e

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
TEST_DIR="$SCRIPT_DIR/tests"

find "$TEST_DIR" -type d -name .dqbuild -prune -exec rm -rf {} +
find "$TEST_DIR" -type f \( \
  -name '*.exe' -o \
  -name '*.o' -o \
  -name '*.dqm_if' -o \
  -name '*.atr' \
\) -exec rm -f {} +
