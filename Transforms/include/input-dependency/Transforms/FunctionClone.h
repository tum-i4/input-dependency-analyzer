#pragma once

#include "input-dependency/Analysis/FunctionCallDepInfo.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

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
    static std::string mask_to_string(const mask& m);

public:
    bool hasCloneForMask(const mask& m) const;
    llvm::Function* getClonedFunction(const mask& m) const;
    llvm::Function* doCloneForMask(const mask& m);

    bool addClone(const mask& m, llvm::Function* F);

    void dump() const;
    
private:
    llvm::Function* m_originalF;
    using clone_info = std::pair<llvm::Function*, llvm::ValueToValueMapTy*>;
    std::unordered_map<mask, clone_info> m_clones;
};

}

