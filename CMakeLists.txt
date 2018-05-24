cmake_minimum_required(VERSION 3.1)

set (DG_INCLUDE_DIR "/usr/local/include/llvm-dg")

find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_LIST_DIR})
include_directories(${DG_INCLUDE_DIR})

link_directories(${LLVM_LIBRARY_DIRS})
link_directories(${CMAKE_CURRENT_BINARY_DIR})

find_package(PkgConfig REQUIRED)
include_directories(${CMAKE_CURRENT_LIST_DIR}/json)

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNDEBUG")

add_definitions(-DHAVE_LLVM)
add_definitions(-DENABLE_CFG)

add_subdirectory(Analysis)  # Use your pass name here.
add_subdirectory(Transforms)  # Use your pass name here.
#add_subdirectory(OH)  # Use your pass name here.
#add_subdirectory(CutVertice)  # Use your pass name here.
