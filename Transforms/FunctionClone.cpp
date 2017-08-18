#include "FunctionClone.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/Cloning.h"

namespace oh {

FunctionClone::FunctionClone(llvm::Function* F)
    : m_originalF(F)
{
}

bool FunctionClone::hasCloneForMask(const mask& m) const
{
    return m_clones.find(m) != m_clones.end();
}

llvm::Function* FunctionClone::getClonedFunction(const mask& m) const
{
    auto pos = m_clones.find(m);
    assert(pos != m_clones.end());
    return pos->second;
}

llvm::Function* FunctionClone::doCloneForMask(const mask& m)
{
    if (hasCloneForMask(m)) {
        return getClonedFunction(m);
    }
    llvm::ValueToValueMapTy VMap;
    llvm::Function* newF = llvm::CloneFunction(m_originalF, VMap);
    m_clones[m] = newF;
    return newF;
}

FunctionClone::mask FunctionClone::createMaskForCall(const input_dependency::FunctionCallDepInfo::ArgumentDependenciesMap& argDeps,
                                                     unsigned size,
                                                     bool is_variadic)
{
    mask callSiteMask(size);
    for (const auto& argDep : argDeps) {
        if (!argDep.first) {
            continue;
        }
        unsigned index = argDep.first->getArgNo();
        if (is_variadic) {
            if (index >= size) {
                callSiteMask.resize(index);
            }
        } else {
            assert(index < size);
        }
        if (argDep.second.isInputIndep()) {
            callSiteMask[index] = false;
        } else if (argDep.second.isInputDep() || argDep.second.isInputArgumentDep()) {
            callSiteMask[index] = true;
        } else {
            assert(false);
        }
    }
    return callSiteMask;
}

}

