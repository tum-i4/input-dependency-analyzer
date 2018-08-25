#include "input-dependency/Transforms/FunctionClone.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/Cloning.h"

namespace oh {

std::string getCloneFunctionName(const std::string& original_name, const FunctionClone::mask& m)
{
    return original_name + FunctionClone::mask_to_string(m);
}


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
    return pos->second.first;
}

llvm::Function* FunctionClone::doCloneForMask(const mask& m)
{
    if (hasCloneForMask(m)) {
        return getClonedFunction(m);
    }
    llvm::ValueToValueMapTy* VMap = new llvm::ValueToValueMapTy();
    llvm::Function* newF = llvm::CloneFunction(m_originalF, *VMap);
    newF->setName(getCloneFunctionName(m_originalF->getName(), m));
    m_clones.insert(std::make_pair(m, clone_info(newF, VMap)));
    return newF;
}

bool FunctionClone::addClone(const mask& m, llvm::Function* F)
{
    return m_clones.insert(std::make_pair(m, clone_info(F, new llvm::ValueToValueMapTy()))).second;
}

void FunctionClone::dump() const
{
    llvm::dbgs() << m_originalF->getName() << "\n";
    for (const auto& clone : m_clones) {
        llvm::dbgs() << "   mask: " <<  mask_to_string(clone.first) << " clone: " << clone.second.first->getName() << "\n";
    }
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

std::string FunctionClone::mask_to_string(const FunctionClone::mask& m)
{
    std::string mask_str;
    for (const auto& b : m) {
        mask_str += std::to_string(b);
    }
    return mask_str;
}

}

