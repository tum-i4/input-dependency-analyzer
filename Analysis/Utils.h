#pragma once

#include "definitions.h"
#include "DependencyAnaliser.h"

namespace llvm {
class Loop;
}

namespace input_dependency {

class Utils {

public:
    static bool haveIntersection(const DependencyAnaliser::ArgumentDependenciesMap& inputNums,
                                 const ArgumentSet& selfNums);

    static ValueSet dissolveInstruction(llvm::Instruction* instr);

    static bool isLibraryFunction(llvm::Function* F, llvm::Module* M);
    static llvm::Loop* getTopLevelLoop(llvm::Loop* loop, llvm::Loop* topParent = nullptr);

    static std::string demangle_name(const std::string& name);
}; // class Utils

} // namespace input_dependency

