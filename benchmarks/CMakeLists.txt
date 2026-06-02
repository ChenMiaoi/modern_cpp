cmake_minimum_required(VERSION 3.16)
project(ModernCppBenchmarks LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-O2 -Wall -Wextra)
endif()

find_package(Threads REQUIRED)

add_executable(string_sso_boundary string_sso_boundary.cpp)
add_executable(vector_vs_list vector_vs_list.cpp)
add_executable(false_sharing false_sharing.cpp)
target_link_libraries(false_sharing Threads::Threads)
