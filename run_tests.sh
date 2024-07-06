#!/bin/bash
set -e

# Must be called from project's root directory like so: ./run_tests.sh

# Function to clean up and return to the original directory
cleanup() {
    popd
}

# Register the cleanup function to be called on EXIT
trap cleanup EXIT

# Configure the project
cmake -B build -S .
# Build only the test target
cmake --build ./build --target redis_tests
# Switch to build directory but save dir so we can cleanup when done.
pushd build
# Run the tests
export GTEST_COLOR=1
ctest --verbose --output-on-failure