cmake_minimum_required(VERSION 3.12)
project(profiler)

set(CMAKE_CXX_STANDARD 14)

add_library(${PROJECT_NAME} STATIC profiler.cpp)
add_executable(test_profiler main.cpp trie.cpp)
target_link_libraries(test_profiler PRIVATE profiler)

if (WIN32)
    target_link_libraries(test_profiler PRIVATE dbgHelp.lib)
elseif (UNIX)
    target_link_libraries(test_profiler PRIVATE "-lpthread")
endif()