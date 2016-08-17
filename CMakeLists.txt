cmake_minimum_required(VERSION 3.5)
project(egitest)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES main.cpp netstation.cpp netstation.h)
add_executable(egitest ${SOURCE_FILES})