cmake_minimum_required(VERSION 3.12)

project(input-dependency VERSION 0.1 LANGUAGES CXX)

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNDEBUG")

add_definitions(-DHAVE_LLVM)
add_definitions(-DENABLE_CFG)

if ($ENV{CLION_IDE})
    include_directories("/usr/include/llvm-6.0/")
    include_directories("/usr/include/llvm-c-6.0/")
endif ()
add_subdirectory(Analysis)  # Use your pass name here.
add_subdirectory(Transforms)  # Use your pass name here.
#add_subdirectory(OH)  # Use your pass name here.
#add_subdirectory(CutVertice)  # Use your pass name here.
