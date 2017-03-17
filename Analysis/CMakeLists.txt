add_library(InputDependency MODULE
    # List your source files here.
    InputDependencyAnalysis.cpp
    FunctionAnaliser.cpp
    BasicBlockAnalysisResult.cpp
    DependencyAnaliser.cpp
    LoopAnalysisResult.cpp
    NonDeterministicBasicBlockAnaliser.cpp
    ReflectingBasicBlockAnaliser.cpp
    NonDeterministicReflectingBasicBlockAnaliser.cpp
    Utils.cpp
    TestPass.cpp
    FunctionDOTGraphPrinter.cpp
)

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
