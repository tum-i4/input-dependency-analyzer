#include "Utils.h"

#include "DependencyInfo.h"

#include "llvm/IR/Argument.h"
#include "llvm/IR/Instructions.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <cxxabi.h>

namespace input_dependency {

bool Utils::haveIntersection(const DependencyAnaliser::ArgumentDependenciesMap& inputNums,
                             const ArgumentSet& selfNums)
{
    DepInfo info;
    for (auto& self : selfNums) {
        auto pos = inputNums.find(self);
        if (pos == inputNums.end()) {
            continue;
        }
        info.mergeDependency(pos->second.getDependency());
        if (info.isInputDep()) {
            return true;
        }
        //return pos->second.isInputDep();
        //|| pos->second.isInputArgumentDep();
    }
    return false;
}

ValueSet Utils::dissolveInstruction(llvm::Instruction* instr)
{
    ValueSet values;
    for (auto op = instr->op_begin(); op != instr->op_end(); ++op) {
        if (auto constop = llvm::dyn_cast<llvm::Constant>(op)) {
            continue;
        }
        if (auto instrop = llvm::dyn_cast<llvm::Instruction>(op)) {
            if (auto allocInstr = llvm::dyn_cast<llvm::AllocaInst>(instrop)) {
                values.insert(llvm::dyn_cast<llvm::Value>(op));
                continue;
            }
            const auto& vals = dissolveInstruction(instrop);
            values.insert(vals.begin(), vals.end());
        } else if (auto val = llvm::dyn_cast<llvm::Value>(op)) {
            if (val->getType()->isLabelTy()) {
                continue;
            }
            values.insert(val);
        }
    }
    return values;
}

bool Utils::isLibraryFunction(llvm::Function* F, llvm::Module* M)
{
    assert(F != nullptr);
    assert(M != nullptr);
    return (F->getParent() != M
            || F->isDeclaration());

    //|| F->getLinkage() == llvm::GlobalValue::LinkOnceODRLinkage);
}


std::string Utils::demangle_name(const std::string& name)
{
    int status = -1;
    char* demangled = abi::__cxa_demangle(name.c_str(), NULL, NULL, &status);
    if (status == 0) {
        return std::string(demangled);
    }
    return std::string();
}

}

