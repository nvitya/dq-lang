#!/bin/bash
export DEBUGINFOD_URLS=""
# Set locale to C to prevent formatting issues in gdb MI parser
export LC_ALL=C
export LANG=C
exec /usr/bin/gdb "$@"
