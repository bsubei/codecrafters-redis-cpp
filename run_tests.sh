#!/bin/bash
set -e

# Must be called from project's root directory like so: ./run_tests.sh

# Function to clean up and return to the original directory
cleanup() {
    popd
}

# Register the cleanup function to be called on EXIT
trap cleanup EXIT

# Configure the project with the special toolchain file (so we can use the newer compiler we installed so we can use concepts).
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=gcc-10-toolchain.cmake
# Build only the test target
cmake --build build --target parser_test
# Switch to build directory but save dir so we can cleanup when done.
pushd build
# Run the tests
export GTEST_COLOR=1
ctest --verbose --output-on-failure