cmake_minimum_required(VERSION 3.12)
project(input-dependency VERSION 0.1 LANGUAGES CXX)

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake)

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


include(GNUInstallDirs)
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/input-dependency)
##
## Config Version file
##
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/input-dependencyConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
        ${CMAKE_CURRENT_LIST_DIR}/cmake/input-dependencyConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/input-dependencyConfig.cmake
        INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/input-dependencyConfigVersion.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/input-dependencyConfig.cmake
        DESTINATION ${INSTALL_CONFIGDIR}
        )

export(PACKAGE input-dependency)