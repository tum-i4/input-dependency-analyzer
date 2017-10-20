#include "InputDependentBasicBlockAnaliser.h"

#include "IndirectCallSitesAnalysis.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

namespace input_dependency {

InputDependentBasicBlockAnaliser::InputDependentBasicBlockAnaliser(llvm::Function* F,
                                                                   llvm::AAResults& AAR,
                                                                   const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                                                                   const IndirectCallSitesAnalysisResult& indirectCallsInfo,
                                                                   const Arguments& inputs,
                                                                   const FunctionAnalysisGetter& Fgetter,
                                                                   llvm::BasicBlock* BB)
                    : BasicBlockAnalysisResult(F, AAR, virtualCallsInfo, indirectCallsInfo, inputs, Fgetter, BB)
{
}

void InputDependentBasicBlockAnaliser::processReturnInstr(llvm::ReturnInst* retInst)
{
    updateInstructionDependencies(retInst, DepInfo(DepInfo::INPUT_DEP));
    llvm::Value* retValue = retInst->getReturnValue();
    if (retValue) {
        ValueDepInfo retValueDepInfo(retValue->getType());
        retValueDepInfo.updateCompositeValueDep(DepInfo(DepInfo::INPUT_DEP));
        updateReturnValueDependencies(retValueDepInfo);
    }
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
    DepInfo info(DepInfo::INPUT_DEP);
    updateInstructionDependencies(storeInst, info);
    ValueDepInfo valueDepInfo(storeTo->getType(), info);
    updateValueDependencies(storeTo, valueDepInfo);
    updateModAliasesDependencies(storeInst, valueDepInfo);
}

DepInfo InputDependentBasicBlockAnaliser::getLoadInstrDependencies(llvm::LoadInst* instr)
{
    return DepInfo(DepInfo::INPUT_DEP);
}

DepInfo InputDependentBasicBlockAnaliser::getInstructionDependencies(llvm::Instruction* instr)
{
    return DepInfo(DepInfo::INPUT_DEP);
}

ValueDepInfo InputDependentBasicBlockAnaliser::getValueDependencies(llvm::Value* value)
{
    auto depInfo = BasicBlockAnalysisResult::getValueDependencies(value);
    if (!depInfo.isDefined()) {
        return ValueDepInfo(value->getType(), DepInfo(DepInfo::INPUT_DEP));
    }
    depInfo.updateCompositeValueDep(DepInfo(DepInfo::INPUT_DEP));
    return depInfo;
}

ValueDepInfo InputDependentBasicBlockAnaliser::getCompositeValueDependencies(llvm::Value* value, llvm::Instruction* element_instr)
{
    auto depInfo = BasicBlockAnalysisResult::getCompositeValueDependencies(value, element_instr);
    if (!depInfo.isDefined()) {
        return ValueDepInfo(value->getType(), DepInfo(DepInfo::INPUT_DEP));
    }
    depInfo.updateCompositeValueDep(DepInfo(DepInfo::INPUT_DEP));
    return depInfo;
}

void InputDependentBasicBlockAnaliser::updateInstructionDependencies(llvm::Instruction* instr, const DepInfo& info)
{
    BasicBlockAnalysisResult::updateInstructionDependencies(instr, DepInfo(DepInfo::INPUT_DEP));
}

void InputDependentBasicBlockAnaliser::updateValueDependencies(llvm::Value* value, const DepInfo& info)
{
    BasicBlockAnalysisResult::updateValueDependencies(value, ValueDepInfo(DepInfo(DepInfo::INPUT_DEP)));
}

void InputDependentBasicBlockAnaliser::updateValueDependencies(llvm::Value* value, const ValueDepInfo& info)
{
    ValueDepInfo newInfo = info;
    newInfo.updateCompositeValueDep(DepInfo(DepInfo::INPUT_DEP));
    BasicBlockAnalysisResult::updateValueDependencies(value, newInfo);
}

void InputDependentBasicBlockAnaliser::updateReturnValueDependencies(const ValueDepInfo& info)
{
    BasicBlockAnalysisResult::updateReturnValueDependencies(ValueDepInfo(DepInfo::INPUT_DEP));
}

ReflectingInputDependentBasicBlockAnaliser::ReflectingInputDependentBasicBlockAnaliser(llvm::Function* F,
                                                                   llvm::AAResults& AAR,
                                                                   const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                                                                   const IndirectCallSitesAnalysisResult& indirectCallsInfo,
                                                                   const Arguments& inputs,
                                                                   const FunctionAnalysisGetter& Fgetter,
                                                                   llvm::BasicBlock* BB)
                    : InputDependentBasicBlockAnaliser(F, AAR, virtualCallsInfo, indirectCallsInfo, inputs, Fgetter, BB)
{
}


}


