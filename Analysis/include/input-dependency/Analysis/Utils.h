#pragma once

#include "input-dependency/Analysis/definitions.h"
#include "input-dependency/Analysis/DependencyAnaliser.h"

namespace llvm {
class Loop;
}

namespace input_dependency {

class Utils {

public:
    static bool isInputDependentForArguments(const DepInfo& depInfo, const DependencyAnaliser::ArgumentDependenciesMap& arg_deps);
    static bool haveIntersection(const DependencyAnaliser::ArgumentDependenciesMap& inputNums,
                                 const ArgumentSet& selfNums);

    static ValueSet dissolveInstruction(llvm::Instruction* instr);

    static bool isLibraryFunction(llvm::Function* F, llvm::Module* M);
    static llvm::Loop* getTopLevelLoop(llvm::Loop* loop, llvm::Loop* topParent = nullptr);
    static int getLoopDepthDiff(llvm::Loop* loop1, llvm::Loop* loop2);

    static std::string demangle_name(const std::string& name);
}; // class Utils

} // namespace input_dependency

