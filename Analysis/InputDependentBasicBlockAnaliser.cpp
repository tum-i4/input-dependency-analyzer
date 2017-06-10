#include "InputDependentBasicBlockAnaliser.h"

#include "VirtualCallSitesAnalysis.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

namespace input_dependency {

InputDependentBasicBlockAnaliser::InputDependentBasicBlockAnaliser(llvm::Function* F,
                                                                   llvm::AAResults& AAR,
                                                                   const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                                                                   const Arguments& inputs,
                                                                   const FunctionAnalysisGetter& Fgetter,
                                                                   llvm::BasicBlock* BB)
                    : BasicBlockAnalysisResult(F, AAR, virtualCallsInfo, inputs, Fgetter, BB)
{
}

void InputDependentBasicBlockAnaliser::processReturnInstr(llvm::ReturnInst* retInst)
{
    updateInstructionDependencies(retInst, DepInfo(DepInfo::INPUT_DEP));
    updateReturnValueDependencies(DepInfo(DepInfo::INPUT_DEP));
}

void InputDependentBasicBlockAnaliser::processBranchInst(llvm::BranchInst* branchInst)
{
    if (branchInst->isUnconditional()) {
        updateInstructionDependencies(branchInst, DepInfo(DepInfo::INPUT_INDEP));
    } else {
        updateInstructionDependencies(branchInst, DepInfo(DepInfo::INPUT_DEP));
    }
}

void InputDependentBasicBlockAnaliser::processStoreInst(llvm::StoreInst* storeInst)
{
    auto storeTo = storeInst->getPointerOperand();
    if (auto global = llvm::dyn_cast<llvm::GlobalVariable>(storeTo)) {
        m_modifiedGlobals.insert(global);
    }
    auto storedValue = getMemoryValue(storeTo);
    assert(storedValue);
    DepInfo info(DepInfo::INPUT_DEP);
    updateInstructionDependencies(storeInst, info);
    updateValueDependencies(storedValue, info);
    updateModAliasesDependencies(storeInst, info);
}

DepInfo InputDependentBasicBlockAnaliser::getLoadInstrDependencies(llvm::LoadInst* instr)
{
    return DepInfo(DepInfo::INPUT_DEP);
}

DepInfo InputDependentBasicBlockAnaliser::getInstructionDependencies(llvm::Instruction* instr)
{
    return DepInfo(DepInfo::INPUT_DEP);
}

DepInfo InputDependentBasicBlockAnaliser::getValueDependencies(llvm::Value* value)
{
    return DepInfo(DepInfo::INPUT_DEP);
}

ReflectingInputDependentBasicBlockAnaliser::ReflectingInputDependentBasicBlockAnaliser(llvm::Function* F,
                                                                   llvm::AAResults& AAR,
                                                                   const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                                                                   const Arguments& inputs,
                                                                   const FunctionAnalysisGetter& Fgetter,
                                                                   llvm::BasicBlock* BB)
                    : InputDependentBasicBlockAnaliser(F, AAR, virtualCallsInfo, inputs, Fgetter, BB)
{
}


}


