#!/bin/bash
set -e

echo "$@"
# Must be called from project's root directory like so: ./run_main.sh
# Configure the project with the special toolchain file (so we can use the newer compiler we installed so we can use concepts).
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=gcc-10-toolchain.cmake
# Build only the test target
cmake --build build --target server
exec ./build/server "$@"