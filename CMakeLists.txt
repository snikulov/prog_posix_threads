#
# CMake build generator PPT book
#

# we're using v2.8+ by default
cmake_minimum_required(VERSION 2.8.0 FATAL_ERROR)

# preferred threading lib is - POSIX
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)

find_package(Threads REQUIRED)

# build alarm_mutex
add_executable(alarm_mutex alarm_mutex.c)
target_link_libraries(alarm_mutex ${CMAKE_THREAD_LIBS_INIT})

# build backoff
add_executable(backoff backoff.c)
target_link_libraries(backoff ${CMAKE_THREAD_LIBS_INIT})