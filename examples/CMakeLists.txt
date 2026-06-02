cmake_minimum_required(VERSION 3.16)
project(ModernCppExamples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific flags
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_compile_options("$<$<CONFIG:Release>:-O2>")
    add_compile_options("$<$<CONFIG:Debug>:-g;-O0>")
elseif(MSVC)
    add_compile_options(/W4 /permissive-)
endif()

# Helper function for single-file examples
function(add_example name source)
    add_executable(${name} ${source})
    target_compile_features(${name} PRIVATE cxx_std_20)
endfunction()

# Object model examples
add_example(object_layout object_model/01_object_layout.cpp)
add_example(strict_aliasing object_model/02_strict_aliasing.cpp)
add_example(ebo object_model/03_ebo.cpp)

# Value category examples
add_example(value_cat_basics value_categories/01_basics.cpp)
add_example(move_semantics value_categories/02_move_semantics.cpp)
add_example(perfect_forwarding value_categories/03_perfect_forwarding.cpp)

# Memory model examples
add_example(relaxed_order memory_model/01_relaxed_order.cpp)
add_example(seq_cst memory_model/02_seq_cst.cpp)
add_example(lock_free_stack memory_model/03_lock_free_stack.cpp)
add_example(false_sharing memory_model/04_false_sharing.cpp)

# Compiler optimization examples
add_example(sroa_demo compiler_opt/01_sroa.cpp)
add_example(inlining_demo compiler_opt/02_inlining.cpp)
add_example(vectorize_demo compiler_opt/03_vectorize.cpp)

# RTTI / vtable examples
add_example(vtable_layout rtti_vtable/01_vtable_layout.cpp)
add_example(dynamic_cast_cost rtti_vtable/02_dynamic_cast.cpp)
add_example(crtp rtti_vtable/03_crtp.cpp)
