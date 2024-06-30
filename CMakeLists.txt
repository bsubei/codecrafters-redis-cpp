cmake_minimum_required(VERSION 3.13)

project(redis-starter-cpp)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp)

#set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fconcepts")
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)
#find_package(asio CONFIG REQUIRED)

# NOTE: header files are already included since they're in the same dir as the source files.
add_executable(server ${SOURCE_FILES})

#target_link_libraries(server PRIVATE asio asio::asio)
target_link_libraries(server PRIVATE Threads::Threads)