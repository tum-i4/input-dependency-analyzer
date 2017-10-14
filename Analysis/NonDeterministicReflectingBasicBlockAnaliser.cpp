#include "NonDeterministicReflectingBasicBlockAnaliser.h"

#include "IndirectCallSitesAnalysis.h"
#include "Utils.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/OperandTraits.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"


namespace input_dependency {

NonDeterministicReflectingBasicBlockAnaliser::NonDeterministicReflectingBasicBlockAnaliser(
                                     llvm::Function* F,
                                     llvm::AAResults& AAR,
                                     const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                                     const IndirectCallSitesAnalysisResult& indirectCallsInfo,
                                     const Arguments& inputs,
                                     const FunctionAnalysisGetter& Fgetter,
                                     llvm::BasicBlock* BB,
                                     const DepInfo& nonDetDeps)
                                : ReflectingBasicBlockAnaliser(F, AAR, virtualCallsInfo, indirectCallsInfo, inputs, Fgetter, BB)
                                , m_nonDeterministicDeps(nonDetDeps)
                                , m_is_final_inputDep(false)
{
}

void NonDeterministicReflectingBasicBlockAnaliser::finalizeResults(const ArgumentDependenciesMap& dependentArgs)
{
    ReflectingBasicBlockAnaliser::finalizeResults(dependentArgs);
    if (m_nonDeterministicDeps.isInputDep()) {
        m_is_final_inputDep = true;
    }
    if (m_nonDeterministicDeps.isInputArgumentDep() && Utils::haveIntersection(dependentArgs, m_nonDeterministicDeps.getArgumentDependencies())) {
        m_is_final_inputDep = true;
    } 
}

DepInfo NonDeterministicReflectingBasicBlockAnaliser::getInstructionDependencies(llvm::Instruction* instr)
{
    auto depInfo = ReflectingBasicBlockAnaliser::getInstructionDependencies(instr);
    if (depInfo.isInputDep()) {
        return depInfo;
    }
    depInfo.mergeDependencies(m_nonDeterministicDeps);
    return depInfo;
}

ValueDepInfo NonDeterministicReflectingBasicBlockAnaliser::getValueDependencies(llvm::Value* value)
{
    auto depInfo = ReflectingBasicBlockAnaliser::getValueDependencies(value);
    if (!depInfo.isDefined()) {
        return depInfo;
    }
    if (depInfo.isInputDep()) {
        return depInfo;
    }
    depInfo.mergeDependencies(m_nonDeterministicDeps);
    return depInfo;
}

ValueDepInfo NonDeterministicReflectingBasicBlockAnaliser::getCompositeValueDependencies(llvm::Value* value, llvm::Instruction* element_instr)
{
    auto depInfo = ReflectingBasicBlockAnaliser::getCompositeValueDependencies(value, element_instr);
    if (!depInfo.isDefined()) {
        return depInfo;
    }
    if (depInfo.isInputDep()) {
        return depInfo;
    }
    depInfo.mergeDependencies(m_nonDeterministicDeps);
    return depInfo;
}

void NonDeterministicReflectingBasicBlockAnaliser::updateValueDependencies(llvm::Value* value, const DepInfo& info)
{
    ReflectingBasicBlockAnaliser::updateValueDependencies(value, info);
}

void NonDeterministicReflectingBasicBlockAnaliser::updateValueDependencies(llvm::Value* value, const ValueDepInfo& info)
{
    ReflectingBasicBlockAnaliser::updateValueDependencies(value, info);
}

void NonDeterministicReflectingBasicBlockAnaliser::updateCompositeValueDependencies(llvm::Value* value,
                                                                                    llvm::Instruction* elInstr,
                                                                                    const ValueDepInfo& info)
{
    ReflectingBasicBlockAnaliser::updateCompositeValueDependencies(value, elInstr, addOnDependencyInfo(info));
}


void NonDeterministicReflectingBasicBlockAnaliser::updateInstructionDependencies(llvm::Instruction* instr, const DepInfo& info)
{
    ReflectingBasicBlockAnaliser::updateInstructionDependencies(instr, addOnDependencyInfo(info));
}

void NonDeterministicReflectingBasicBlockAnaliser::updateReturnValueDependencies(const ValueDepInfo& info)
{
    ReflectingBasicBlockAnaliser::updateReturnValueDependencies(addOnDependencyInfo(info));
}

ValueDepInfo NonDeterministicReflectingBasicBlockAnaliser::getArgumentValueDependecnies(llvm::Value* argVal)
{
    auto depInfo = ReflectingBasicBlockAnaliser::getArgumentValueDependecnies(argVal);
    addOnDependencyInfo(depInfo);
    return depInfo;
}

DepInfo NonDeterministicReflectingBasicBlockAnaliser::addOnDependencyInfo(const DepInfo& info)
{
    if (info.isInputDep()) {
        return info;
    }
    auto newInfo = info;
    newInfo.mergeDependencies(m_nonDeterministicDeps);
    return newInfo;
}

ValueDepInfo NonDeterministicReflectingBasicBlockAnaliser::addOnDependencyInfo(const ValueDepInfo& info)
{
    auto newInfo = info;
    ValueDepInfo nonDetDeps_info = newInfo;
    nonDetDeps_info.updateCompositeValueDep(m_nonDeterministicDeps);
    newInfo.mergeDependencies(nonDetDeps_info);
    return newInfo;
}


} // namespace input_dependency

