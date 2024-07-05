cmake_minimum_required(VERSION 3.13)

project(redis-starter-cpp)

# All cpp files, except main.cpp.
file(GLOB_RECURSE SOURCE_FILES src/*.cpp)
list(FILTER SOURCE_FILES EXCLUDE REGEX "^main\\.cpp$")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fconcepts")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Add the third_party directory to the include path
include_directories(${CMAKE_SOURCE_DIR}/third_party)

# GTest library and unit tests
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/f8d7d77c06936315286eb55f8de22cd23c188571.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
enable_testing()
add_executable(parser_test tests/parser_test.cpp)
target_link_libraries(parser_test PRIVATE redis_lib GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(parser_test)
#find_package(GTest REQUIRED)
#include_directories(${GTEST_INCLUDE_DIRS})

# TODO turn these on very soon once the other CMake issues are resolved.
# Add somewhat strict warning flags. This applies to all targets defined after here.
if(MSVC)
  #add_compile_options(/W4 /WX)
else()
  #add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# NOTE: header files are already included since they're in the same dir as the source files.
add_library(redis_lib ${SOURCE_FILES})
target_link_libraries(redis_lib PRIVATE Threads::Threads)

# Main executable called "server"
add_executable(server ${SOURCE_FILES} "src/main.cpp")
target_link_libraries(server PRIVATE redis_lib)

# Define custom target to clean the build directory
add_custom_target(clean-build
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMENT "Cleaning the build directory"
)