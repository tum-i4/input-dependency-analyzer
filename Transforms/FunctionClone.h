#pragma once

#include "Analysis/FunctionCallDepInfo.h"

#include <vector>

namespace llvm {
class Instruction;
class CallInst;
class InvokeInst;
}

namespace oh {

class FunctionClone
{
public:
    using mask = std::vector<bool>;

public:
    FunctionClone(llvm::Function* F);

    FunctionClone(FunctionClone&&) = default;
    FunctionClone& operator= (FunctionClone&&) = delete;

    FunctionClone(const FunctionClone&) = delete;
    FunctionClone& operator= (const FunctionClone&) = delete;

public:
    static mask createMaskForCall(const input_dependency::FunctionCallDepInfo::ArgumentDependenciesMap& argDeps,
                                  unsigned size,
                                  bool is_variadic);

public:
    bool hasCloneForMask(const mask& m) const;
    llvm::Function* getClonedFunction(const mask& m) const;
    llvm::Function* doCloneForMask(const mask& m);
    
private:
    llvm::Function* m_originalF;
    std::unordered_map<mask, llvm::Function*> m_clones;
};

}

