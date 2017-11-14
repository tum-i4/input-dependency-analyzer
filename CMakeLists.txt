cmake_minimum_required(VERSION 3.1)

find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_LIST_DIR})
link_directories(${LLVM_LIBRARY_DIRS})
link_directories(${CMAKE_CURRENT_BINARY_DIR})

find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP jsoncpp)
link_directories(${JSONCPP_LIBRARY_DIRS})
include_directories(${JSONCPP_INCLUDE_DIRS})

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNDEBUG")

add_subdirectory(Analysis)  # Use your pass name here.
add_subdirectory(Transforms)  # Use your pass name here.
add_subdirectory(OH)  # Use your pass name here.
#add_subdirectory(CutVertice)  # Use your pass name here.
