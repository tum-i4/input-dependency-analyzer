cmake_minimum_required(VERSION 3.1)

set (SVF_INCLUDE_DIR "/usr/local/include/SVF")
set (SVF_LIB_DIR "/usr/local/lib")

find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_LIST_DIR})
include_directories(${SVF_INCLUDE_DIR})

link_directories(${LLVM_LIBRARY_DIRS})
link_directories(${CMAKE_CURRENT_BINARY_DIR})
link_directories(${SVF_LIB_DIR})

find_package(PkgConfig REQUIRED)
include_directories(${CMAKE_CURRENT_LIST_DIR}/json)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNDEBUG")

add_subdirectory(Analysis)  # Use your pass name here.
add_subdirectory(Transforms)  # Use your pass name here.
add_subdirectory(lib)  # Use your pass name here.
