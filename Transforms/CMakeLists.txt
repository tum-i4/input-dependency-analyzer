cmake_minimum_required(VERSION 3.12)

project(input-dependency-transforms VERSION 0.1 LANGUAGES CXX)

add_library(Transforms SHARED
        include/input-dependency/Transforms/FunctionClone.h
        include/input-dependency/Transforms/FunctionClonePass.h
        include/input-dependency/Transforms/FunctionExtraction.h
        include/input-dependency/Transforms/FunctionSnippet.h
        include/input-dependency/Transforms/Utils.h

        src/FunctionClonePass.cpp
        src/FunctionClone.cpp
        src/FunctionExtraction.cpp
        src/FunctionSnippet.cpp
        src/Utils.cpp)

add_library(input-dependency::Transforms ALIAS Transforms)

find_package(LLVM 6.0 REQUIRED CONFIG)

target_include_directories(Transforms
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${LLVM_INCLUDE_DIRS})


target_compile_features(Transforms PRIVATE cxx_range_for cxx_auto_type)
target_compile_options(Transforms PRIVATE -fno-rtti)
target_link_libraries(Transforms PRIVATE input-dependency::InputDependency)

# Get proper shared-library behavior (where symbols are not necessarily
# resolved when the shared library is linked) on OS X.
if (APPLE)
    set_target_properties(Transforms PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
endif (APPLE)



##
## INSTALL
## install header files, generate and install cmake config files for find_package()
##
include(GNUInstallDirs)
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/input-dependency)

install(
        TARGETS Transforms
        EXPORT input-dependencyTransformsTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT Transforms
)

install(
        DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT Transforms
)

install(
        EXPORT input-dependencyTransformsTargets
        NAMESPACE input-dependency::
        DESTINATION ${INSTALL_CONFIGDIR}
        COMPONENT Transforms
)


##
## Export
##
export(
        EXPORT input-dependencyTransformsTargets
        NAMESPACE input-dependency::
        FILE ${CMAKE_CURRENT_BINARY_DIR}/input-dependencyTransformsTargets.cmake
)

export(PACKAGE input-dependency-transforms)