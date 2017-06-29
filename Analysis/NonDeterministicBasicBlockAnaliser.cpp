#include "NonDeterministicBasicBlockAnaliser.h"

#include "IndirectCallSitesAnalysis.h"

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

NonDeterministicBasicBlockAnaliser::NonDeterministicBasicBlockAnaliser(
                        llvm::Function* F,
                        llvm::AAResults& AAR,
                        const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                        const IndirectCallSitesAnalysisResult& indirectCallsInfo,
                        const Arguments& inputs,
                        const FunctionAnalysisGetter& Fgetter,
                        llvm::BasicBlock* BB,
                        const DepInfo& nonDetArgs)
                    : BasicBlockAnalysisResult(F, AAR, virtualCallsInfo, indirectCallsInfo, inputs, Fgetter, BB)
                    , m_nonDetDeps(nonDetArgs)
{
}


DepInfo NonDeterministicBasicBlockAnaliser::getInstructionDependencies(llvm::Instruction* instr)
{
    auto depInfo = BasicBlockAnalysisResult::getInstructionDependencies(instr);
    depInfo.mergeDependencies(m_nonDetDeps);
    return depInfo;
}

DepInfo NonDeterministicBasicBlockAnaliser::getValueDependencies(llvm::Value* value)
{
    auto depInfo = BasicBlockAnalysisResult::getValueDependencies(value);
    if (!depInfo.isDefined()) {
        return depInfo;
    }
    depInfo.mergeDependencies(m_nonDetDeps);
    return depInfo;
}

void NonDeterministicBasicBlockAnaliser::updateInstructionDependencies(llvm::Instruction* instr, const DepInfo& info)
{
    BasicBlockAnalysisResult::updateInstructionDependencies(instr, addOnDependencyInfo(info));
}

void NonDeterministicBasicBlockAnaliser::updateValueDependencies(llvm::Value* value, const DepInfo& info)
{
    BasicBlockAnalysisResult::updateValueDependencies(value, addOnDependencyInfo(info));
}

void NonDeterministicBasicBlockAnaliser::updateReturnValueDependencies(const DepInfo& info)
{
    BasicBlockAnalysisResult::updateReturnValueDependencies(addOnDependencyInfo(info));
}

void NonDeterministicBasicBlockAnaliser::setInitialValueDependencies(const DependencyAnaliser::ValueDependencies& valueDependencies)
{
    BasicBlockAnalysisResult::setInitialValueDependencies(valueDependencies);
    for (auto& dep : m_nonDetDeps.getValueDependencies()) {
        auto pos = valueDependencies.find(dep);
        // e.g. global which has not been seen before, but is referenced at this point
        if (pos != valueDependencies.end()) {
            m_valueDependencies[dep] = pos->second;
        } else if (auto global = llvm::dyn_cast<llvm::GlobalVariable>(dep)) {
            m_referencedGlobals.insert(global);
        }
    }
}

DepInfo NonDeterministicBasicBlockAnaliser::getArgumentValueDependecnies(llvm::Value* argVal)
{
    auto depInfo = BasicBlockAnalysisResult::getArgumentValueDependecnies(argVal);
    addOnDependencyInfo(depInfo);
    return depInfo;
}

DepInfo NonDeterministicBasicBlockAnaliser::addOnDependencyInfo(const DepInfo& info)
{
    auto newInfo = info;
    newInfo.mergeDependencies(m_nonDetDeps);
    return newInfo;
}

} // namespace input_dependency

