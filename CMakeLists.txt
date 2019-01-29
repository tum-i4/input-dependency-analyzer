cmake_minimum_required(VERSION 3.12)
project(input_dependency VERSION 0.1.0 LANGUAGES CXX)

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake)
find_package(LLVM 6.0 REQUIRED CONFIG)
find_package (Threads)
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(svf REQUIRED COMPONENTS Svf)
find_package(pdg REQUIRED)

add_definitions(-DHAVE_LLVM)
add_definitions(-DENABLE_CFG)

add_library(InputDependency MODULE
            lib/PDG/GraphBuilder.cpp
            lib/analysis/ReachabilityAnalysis.cpp
            lib/analysis/ArgumentReachabilityAnalysis.cpp
            lib/analysis/InputDependencyReachabilityAnalysis.cpp
            lib/analysis/InputDependencySources.cpp
            lib/analysis/LibFunctionInfo.cpp
            lib/analysis/LibraryInfoCollector.cpp
            lib/analysis/CLibraryInfo.cpp
            lib/analysis/STLStringInfo.cpp
            lib/analysis/LLVMIntrinsicsInfo.cpp
            lib/analysis/LibraryInfoFromConfigFile.cpp
            lib/analysis/LibraryInfoManager.cpp
            lib/analysis/InputDependencyAnalysis.cpp
            lib/passes/GraphBuilderPass.cpp
            lib/passes/ArgumentReachabilityAnalysisPass.cpp
            lib/passes/InputDependencyReachabilityAnalysisPass.cpp
            lib/utils/Utils.cpp
            lib/Debug/PDGPrinter.cpp)

target_include_directories(InputDependency PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${LLVM_INCLUDE_DIRS}
        ${pdg_INCLUDE_DIR}
)

target_link_libraries(InputDependency PRIVATE
                      Threads::Threads
                      svf::Svf
                      nlohmann_json::nlohmann_json
)

target_compile_features(InputDependency PRIVATE cxx_range_for cxx_auto_type cxx_std_17)
target_compile_options(InputDependency PRIVATE -fno-rtti -g)

