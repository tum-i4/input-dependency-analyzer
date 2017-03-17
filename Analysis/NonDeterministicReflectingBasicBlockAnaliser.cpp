#include "NonDeterministicReflectingBasicBlockAnaliser.h"

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
                                     const Arguments& inputs,
                                     const FunctionAnalysisGetter& Fgetter,
                                     llvm::BasicBlock* BB,
                                     const DepInfo& nonDetDeps)
                                : ReflectingBasicBlockAnaliser(F, AAR, inputs, Fgetter, BB)
                                , m_nonDeterministicDeps(nonDetDeps)
{
}

DepInfo NonDeterministicReflectingBasicBlockAnaliser::getInstructionDependencies(llvm::Instruction* instr)
{
    auto depInfo = ReflectingBasicBlockAnaliser::getInstructionDependencies(instr);
    depInfo.mergeDependencies(m_nonDeterministicDeps);
    return depInfo;
}

DepInfo NonDeterministicReflectingBasicBlockAnaliser::getValueDependencies(llvm::Value* value)
{
    auto depInfo = ReflectingBasicBlockAnaliser::getValueDependencies(value);
    depInfo.mergeDependencies(m_nonDeterministicDeps);
    return depInfo;
}

void NonDeterministicReflectingBasicBlockAnaliser::updateInstructionDependencies(llvm::Instruction* instr, const DepInfo& info)
{
    ReflectingBasicBlockAnaliser::updateInstructionDependencies(instr, addOnDependencyInfo(info));
}

void NonDeterministicReflectingBasicBlockAnaliser::updateValueDependencies(llvm::Value* value, const DepInfo& info)
{
    ReflectingBasicBlockAnaliser::updateValueDependencies(value, addOnDependencyInfo(info));
}

void NonDeterministicReflectingBasicBlockAnaliser::updateReturnValueDependencies(const DepInfo& info)
{
    ReflectingBasicBlockAnaliser::updateReturnValueDependencies(addOnDependencyInfo(info));
}

DepInfo NonDeterministicReflectingBasicBlockAnaliser::addOnDependencyInfo(const DepInfo& info)
{
    auto newInfo = info;
    newInfo.mergeDependencies(m_nonDeterministicDeps);
    return newInfo;
}

} // namespace input_dependency

