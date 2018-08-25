cmake_minimum_required(VERSION 3.12)

project(input-dependency-analysis VERSION 0.1 LANGUAGES CXX)

add_library(InputDependency SHARED
        include/input-dependency/Analysis/BasicBlockAnalysisResult.h
        include/input-dependency/Analysis/BasicBlocksUtils.h
        include/input-dependency/Analysis/CachedFunctionAnalysisResult.h
        include/input-dependency/Analysis/CachedInputDependencyAnalysis.h
        include/input-dependency/Analysis/CFGTraversalPath.h
        include/input-dependency/Analysis/CLibraryInfo.h
        include/input-dependency/Analysis/ClonedFunctionAnalysisResult.h
        include/input-dependency/Analysis/constants.h
        include/input-dependency/Analysis/definitions.h
        include/input-dependency/Analysis/DependencyAnaliser.h
        include/input-dependency/Analysis/DependencyAnalysisResult.h
        include/input-dependency/Analysis/DependencyInfo.h
        include/input-dependency/Analysis/dot_interfaces.h
        include/input-dependency/Analysis/DotPrinter.h
        include/input-dependency/Analysis/exception.h
        include/input-dependency/Analysis/FunctionAnaliser.h
        include/input-dependency/Analysis/FunctionCallDepInfo.h
        include/input-dependency/Analysis/FunctionDominanceTree.h
        include/input-dependency/Analysis/FunctionDOTGraphPrinter.h
        include/input-dependency/Analysis/FunctionInputDependencyResultInterface.h
        include/input-dependency/Analysis/IndirectCallSitesAnalysis.h
        include/input-dependency/Analysis/InputDepConfig.h
        include/input-dependency/Analysis/InputDependencyAnalysis.h
        include/input-dependency/Analysis/InputDependencyAnalysisInterface.h
        include/input-dependency/Analysis/InputDependencyDebugInfoPrinter.h
        include/input-dependency/Analysis/InputDependencyStatistics.h
        include/input-dependency/Analysis/InputDependentBasicBlockAnaliser.h
        include/input-dependency/Analysis/InputDependentFunctionAnalysisResult.h
        include/input-dependency/Analysis/InputDependentFunctions.h
        include/input-dependency/Analysis/InputDepInstructionsRecorder.h
        include/input-dependency/Analysis/LibFunctionInfo.h
        include/input-dependency/Analysis/LibraryInfoCollector.h
        include/input-dependency/Analysis/LibraryInfoFromConfigFile.h
        include/input-dependency/Analysis/LibraryInfoManager.h
        include/input-dependency/Analysis/LLVMIntrinsicsInfo.h
        include/input-dependency/Analysis/LoggingUtils.h
        include/input-dependency/Analysis/LoopAnalysisResult.h
        include/input-dependency/Analysis/LoopTraversalPath.h
        include/input-dependency/Analysis/NonDeterministicBasicBlockAnaliser.h
        include/input-dependency/Analysis/NonDeterministicReflectingBasicBlockAnaliser.h
        include/input-dependency/Analysis/ReflectingBasicBlockAnaliser.h
        include/input-dependency/Analysis/ReflectingDependencyAnaliser.h
        include/input-dependency/Analysis/SnakeLibraryInfo.h
        include/input-dependency/Analysis/Statistics.h
        include/input-dependency/Analysis/STLStringInfo.h
        include/input-dependency/Analysis/TransparentCachingPass.h
        include/input-dependency/Analysis/Utils.h
        include/input-dependency/Analysis/value_dependence_graph.h
        include/input-dependency/Analysis/ValueDepInfo.h
        include/input-dependency/Analysis/ReachableFunctions.h

        src/BasicBlockAnalysisResult.cpp
        src/CLibraryInfo.cpp
        src/DependencyAnaliser.cpp
        src/FunctionAnaliser.cpp
        src/CachedFunctionAnalysisResult.cpp
        src/ClonedFunctionAnalysisResult.cpp
        src/FunctionCallDepInfo.cpp
        src/FunctionDOTGraphPrinter.cpp
        src/IndirectCallSitesAnalysis.cpp
        src/InputDependencyAnalysisPass.cpp
        src/InputDependencyAnalysis.cpp
        src/CachedInputDependencyAnalysis.cpp
        src/InputDependencyDebugInfoPrinter.cpp
        src/InputDependencyStatistics.cpp
        #src/InputDependentBasicBlockAnaliser.cpp
        src/LibFunctionInfo.cpp
        src/LibraryFunctionsDebugPass.cpp
        src/LibraryInfoCollector.cpp
        src/LibraryInfoManager.cpp
        src/LoopAnalysisResult.cpp
        src/ModuleSizeDebugPass.cpp
        src/NonDeterministicBasicBlockAnaliser.cpp
        src/NonDeterministicReflectingBasicBlockAnaliser.cpp
        src/ReflectingBasicBlockAnaliser.cpp
        src/STLStringInfo.cpp
        src/LoggingUtils.cpp
        src/Utils.cpp
        src/InputDepInstructionsRecorder.cpp
        src/DotPrinter.cpp
        src/value_dependence_graph.cpp
        src/FunctionDominanceTree.cpp
        src/ValueDepInfo.cpp
        src/LoopTraversalPath.cpp
        src/CFGTraversalPath.cpp
        src/LLVMIntrinsicsInfo.cpp
        src/BasicBlocksUtils.cpp
        src/LibraryInfoFromConfigFile.cpp
        src/Statistics.cpp
        src/constants.cpp
        src/TransparentCachingPass.cpp
        src/ReachableFunctions.cpp)

add_library(input-dependency::InputDependency ALIAS InputDependency)


find_package(LLVM 6.0 REQUIRED CONFIG)
find_package(nlohmann_json REQUIRED)

target_include_directories(InputDependency
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${LLVM_INCLUDE_DIRS})


target_compile_features(InputDependency PRIVATE cxx_range_for cxx_auto_type)
target_compile_options(InputDependency PRIVATE -fno-rtti)

target_link_libraries(InputDependency PRIVATE nlohmann_json::nlohmann_json)


# Get proper shared-library behavior (where symbols are not necessarily
# resolved when the shared library is linked) on OS X.
if (APPLE)
    set_target_properties(InputDependency PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
endif (APPLE)
