#!/bin/sh

set -e

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
RUNNER="$REPO_DIR/build/dq-atrun"

if [ ! -x "$RUNNER" ] && [ -x "$REPO_DIR/autotest/atrunner/build/dq-atrun" ]; then
  RUNNER="$REPO_DIR/autotest/atrunner/build/dq-atrun"
fi

"$RUNNER" \
  -c "$REPO_DIR/build/dq-comp" \
  -r "$REPO_DIR/autotest/tests"
