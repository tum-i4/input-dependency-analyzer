add_library(InputDependency MODULE
    BasicBlockAnalysisResult.cpp
    CLibraryInfo.cpp
    DependencyAnaliser.cpp
    FunctionAnaliser.cpp
    CachedFunctionAnalysisResult.cpp
    ClonedFunctionAnalysisResult.cpp
    FunctionCallDepInfo.cpp
    FunctionDOTGraphPrinter.cpp
    IndirectCallSitesAnalysis.cpp
    InputDependencyAnalysisPass.cpp
    InputDependencyAnalysis.cpp
    CachedInputDependencyAnalysis.cpp
    InputDependencyDebugInfoPrinter.cpp
    InputDependencyStatistics.cpp
    #InputDependentBasicBlockAnaliser.cpp
    LibFunctionInfo.cpp
    LibraryFunctionsDebugPass.cpp
    LibraryInfoCollector.cpp
    LibraryInfoManager.cpp
    LoopAnalysisResult.cpp
    ModuleSizeDebugPass.cpp
    NonDeterministicBasicBlockAnaliser.cpp
    NonDeterministicReflectingBasicBlockAnaliser.cpp
    ReflectingBasicBlockAnaliser.cpp
    STLStringInfo.cpp
    LoggingUtils.cpp
    Utils.cpp
    InputDepInstructionsRecorder.cpp
    DotPrinter.cpp
    value_dependence_graph.cpp
    FunctionDominanceTree.cpp
    ValueDepInfo.cpp
    LoopTraversalPath.cpp
    CFGTraversalPath.cpp
    LLVMIntrinsicsInfo.cpp
    BasicBlocksUtils.cpp
    LibraryInfoFromConfigFile.cpp
    Statistics.cpp
    constants.cpp
    TransparentCachingPass.cpp
)

install(DIRECTORY ./ DESTINATION /usr/local/include/input-dependency
        FILES_MATCHING PATTERN "*.h")
install(TARGETS InputDependency LIBRARY DESTINATION /usr/local/lib)

# Use C++11 to compile our pass (i.e., supply -std=c++11).
target_compile_features(InputDependency PRIVATE cxx_range_for cxx_auto_type)

# LLVM is (typically) built with no C++ RTTI. We need to match that;
# otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(InputDependency PROPERTIES
    COMPILE_FLAGS "-fno-rtti -g"
)

# Get proper shared-library behavior (where symbols are not necessarily
# resolved when the shared library is linked) on OS X.
if(APPLE)
    set_target_properties(InputDependency PROPERTIES
            LINK_FLAGS "-undefined dynamic_lookup"
            )
endif(APPLE)
