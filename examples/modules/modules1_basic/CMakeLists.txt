cmake_minimum_required(VERSION 3.28)
project(modules_basic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# CMake 3.28+ 原生支持 C++ modules
add_executable(modules_basic
    main.cpp
    math.cppm
)

target_sources(modules_basic
    PUBLIC FILE_SET CXX_MODULES FILES
    math.cppm
)
