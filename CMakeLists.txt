cmake_minimum_required(VERSION 3.29)

project(redis-starter-cpp)

# All cpp files, except main.cpp.
file(GLOB_RECURSE SOURCE_FILES src/*.cpp)
list(FILTER SOURCE_FILES EXCLUDE REGEX "main\\.cpp$")
# All cpp files in tests dir.
file(GLOB_RECURSE TEST_FILES tests/*.cpp)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# TODO was trying to get msan to work, this was one of the things I tried.
# set(CMAKE_CXX_EXTENSIONS OFF)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Add the third_party directory to the include path
include_directories(${CMAKE_SOURCE_DIR}/third_party)

# Add somewhat strict warning flags. This applies to all targets defined below this point (we want this to be everything).
if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# GTest library and unit test targets.
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/f8d7d77c06936315286eb55f8de22cd23c188571.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
enable_testing()
add_executable(redis_tests ${TEST_FILES})
target_link_libraries(redis_tests PRIVATE redis_lib GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(redis_tests)

# Most of our code is in this library target.
add_library(redis_lib ${SOURCE_FILES})
target_link_libraries(redis_lib PRIVATE Threads::Threads)

# Main executable called "server".
add_executable(server ${SOURCE_FILES} "src/main.cpp")
target_link_libraries(server PRIVATE redis_lib)

# Options for sanitizers.
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)
option(ENABLE_TSAN "Enable Thread Sanitizer" OFF)
option(ENABLE_UBSAN "Enable Undefined Behavior Sanitizer" OFF)
option(ENABLE_MSAN "Enable Memory Sanitizer" OFF)

# Add sanitizer flags based on the provided options. These are passed in by the Makefile rules.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  if(ENABLE_ASAN)
    set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
  endif()
  if(ENABLE_TSAN)
    set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize=thread")
  endif()
  if(ENABLE_UBSAN)
    set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize=undefined")
  endif()
  if(ENABLE_MSAN AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize=memory -fno-omit-frame-pointer -fsanitize-memory-track-origins=2")
  endif()

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND (ENABLE_ASAN OR ENABLE_TSAN OR ENABLE_UBSAN OR ENABLE_MSAN))
  # Add the sanitizer blacklist file.
    target_compile_options(server PRIVATE -fsanitize-blacklist=${CMAKE_SOURCE_DIR}/san_blacklist.txt)
  endif()

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_FLAGS}")
endif()