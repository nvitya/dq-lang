#!/bin/sh

set -u

# Number of successful test runs required. Override with: RUNS=10 ./autotest/repeat-test.sh
RUNS=${RUNS:-100}

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

case "$RUNS" in
  ''|*[!0-9]*|0)
    echo "ERROR: RUNS must be a positive integer (got '$RUNS')." >&2
    exit 2
    ;;
esac

run=1
while [ "$run" -le "$RUNS" ]; do
  printf '\n=== Running make test (%d/%d) ===\n' "$run" "$RUNS"

  if make -C "$REPO_DIR" test; then
    run=$((run + 1))
  else
    status=$?
    printf '\nERROR: make test_O0 failed on run %d of %d.\n' "$run" "$RUNS" >&2
    exit "$status"
  fi
done

printf '\nSUCCESS: make test_O0 passed all %d runs.\n' "$RUNS"
