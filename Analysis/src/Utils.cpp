#include "input-dependency/Analysis/Utils.h"

#include "input-dependency/Analysis/DependencyInfo.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Instructions.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <cxxabi.h>

namespace input_dependency {

bool Utils::isInputDependentForArguments(const DepInfo& depInfo, const DependencyAnaliser::ArgumentDependenciesMap& arg_deps)
{
    if (depInfo.isInputArgumentDep() || !depInfo.getArgumentDependencies().empty()) {
        return haveIntersection(arg_deps, depInfo.getArgumentDependencies());
    }
    if (depInfo.isInputIndep()) {
        return false;
    }
    return true;
}

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
        // make sure this comes before constant check, as global variable inherits from constant
        if (auto global = llvm::dyn_cast<llvm::GlobalVariable>(op)) {
            values.insert(global);
            continue;
        }
        if (auto constop = llvm::dyn_cast<llvm::Constant>(op)) {
            continue;
        }
        if (auto instrop = llvm::dyn_cast<llvm::Instruction>(op)) {
            if (auto allocInstr = llvm::dyn_cast<llvm::AllocaInst>(instrop)) {
                values.insert(llvm::dyn_cast<llvm::Value>(op));
                continue;
            } else if (auto callInstr = llvm::dyn_cast<llvm::CallInst>(instrop)) {
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

llvm::Loop* Utils::getTopLevelLoop(llvm::Loop* loop, llvm::Loop* topParent)
{
    while (loop->getParentLoop() != topParent) {
        loop = loop->getParentLoop();
    }
    return loop;
}

int Utils::getLoopDepthDiff(llvm::Loop* loop1, llvm::Loop* loop2)
{
    if (loop1 == nullptr || loop2 == nullptr) {
        return 0;
    }
    // assume loops are in the same loop hierarchy
    return loop1->getLoopDepth() - loop2->getLoopDepth();
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

