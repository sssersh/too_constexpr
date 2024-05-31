cmake_minimum_required(VERSION 3.24)
project(cant)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++2b")
else ()
    set (CMAKE_CXX_STANDARD 23)
endif()

add_subdirectory(examples)
