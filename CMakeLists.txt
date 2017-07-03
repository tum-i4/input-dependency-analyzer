cmake_minimum_required(VERSION 3.1)


find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_LIST_DIR})
link_directories(${LLVM_LIBRARY_DIRS})
link_directories(${CMAKE_CURRENT_BINARY_DIR})

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

add_subdirectory(Analysis)  # Use your pass name here.
