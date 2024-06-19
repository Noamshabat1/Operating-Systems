cmake_minimum_required(VERSION 3.22)
project(Ex1_OS)

set(CMAKE_CXX_STANDARD 11)

add_executable(Ex1_OS
        measure.cpp
        measure.h
        memory_latency.cpp
        memory_latency.h)
