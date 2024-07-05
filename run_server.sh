#!/bin/bash
set -e

# Must be called from project's root directory like so: ./run_main.sh

# Configure the project
cmake -B build -S .
# Build only the test target
cmake --build build --target server
exec ./build/server "$@"