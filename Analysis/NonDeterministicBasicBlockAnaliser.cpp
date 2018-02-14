#include "NonDeterministicBasicBlockAnaliser.h"

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

DepInfo NonDeterministicBasicBlockAnaliser::getBlockDependencies() const
{
    return m_nonDetDeps;
}

void NonDeterministicBasicBlockAnaliser::finalizeResults(const ArgumentDependenciesMap& dependentArgs)
{
    BasicBlockAnalysisResult::finalizeResults(dependentArgs);
    if (m_nonDetDeps.isInputDep()) {
        m_is_inputDep = true;
    }
    if (m_nonDetDeps.isInputArgumentDep() && Utils::haveIntersection(dependentArgs, m_nonDetDeps.getArgumentDependencies())) {
        m_is_inputDep = true;
    } 
}

void NonDeterministicBasicBlockAnaliser::finalizeGlobals(const GlobalVariableDependencyMap& globalsDeps)
{
    BasicBlockAnalysisResult::finalizeGlobals(globalsDeps);
    if (!m_nonDetDeps.isValueDep() && m_nonDetDeps.getValueDependencies().empty()) {
        return;
    }
    finalizeValueDependencies(globalsDeps, m_nonDetDeps);
    m_is_inputDep |= m_nonDetDeps.isInputDep();
}

bool NonDeterministicBasicBlockAnaliser::isInputDependent(llvm::BasicBlock* block,
                                                          const DependencyAnaliser::ArgumentDependenciesMap& depArgs) const
{
    assert(block == m_BB);
    if (m_nonDetDeps.isInputDep() && m_nonDetDeps.getArgumentDependencies().empty()) {
        return true;
    }
    if (depArgs.empty()) {
        return false;
    }
    return Utils::isInputDependentForArguments(m_nonDetDeps, depArgs);
}

bool NonDeterministicBasicBlockAnaliser::isDataDependent(llvm::Instruction* I) const
{
    // Is input dependent and dependency result is not the same as block dependency
    auto pos = m_inputDependentInstrs.find(I);
    if (pos == m_inputDependentInstrs.end()) {
        return false;
    }
    return BasicBlockAnalysisResult::isInputDependent(I) && pos->second != m_nonDetDeps;
}

DepInfo NonDeterministicBasicBlockAnaliser::getInstructionDependencies(llvm::Instruction* instr)
{
    auto depInfo = BasicBlockAnalysisResult::getInstructionDependencies(instr);
    depInfo.mergeDependencies(m_nonDetDeps);
    return depInfo;
}

ValueDepInfo NonDeterministicBasicBlockAnaliser::getValueDependencies(llvm::Value* value)
{
    auto depInfo = BasicBlockAnalysisResult::getValueDependencies(value);
    if (!depInfo.isDefined()) {
        return depInfo;
    }
    depInfo.mergeDependencies(m_nonDetDeps);
    return depInfo;
}

ValueDepInfo NonDeterministicBasicBlockAnaliser::getCompositeValueDependencies(llvm::Value* value, llvm::Instruction* element_instr)
{
    auto depInfo = BasicBlockAnalysisResult::getCompositeValueDependencies(value, element_instr);
    if (!depInfo.isDefined()) {
        return depInfo;
    }
    depInfo.mergeDependencies(m_nonDetDeps);
    return depInfo;
}

void NonDeterministicBasicBlockAnaliser::updateValueDependencies(llvm::Value* value, const DepInfo& info, bool update_aliases)
{
    BasicBlockAnalysisResult::updateValueDependencies(value, addOnDependencyInfo(info), update_aliases);
}

void NonDeterministicBasicBlockAnaliser::updateValueDependencies(llvm::Value* value, const ValueDepInfo& info, bool update_aliases)
{
    BasicBlockAnalysisResult::updateValueDependencies(value, addOnDependencyInfo(info), update_aliases);
}

void NonDeterministicBasicBlockAnaliser::updateCompositeValueDependencies(llvm::Value* value,
                                                                          llvm::Instruction* elInstr,
                                                                          const ValueDepInfo& info)
{
    BasicBlockAnalysisResult::updateCompositeValueDependencies(value, elInstr, addOnDependencyInfo(info));
}

void NonDeterministicBasicBlockAnaliser::updateInstructionDependencies(llvm::Instruction* instr, const DepInfo& info)
{
    BasicBlockAnalysisResult::updateInstructionDependencies(instr, addOnDependencyInfo(info));
}

void NonDeterministicBasicBlockAnaliser::updateReturnValueDependencies(const ValueDepInfo& info)
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

ValueDepInfo NonDeterministicBasicBlockAnaliser::getArgumentValueDependecnies(llvm::Value* argVal)
{
    auto depInfo = BasicBlockAnalysisResult::getArgumentValueDependecnies(argVal);
    if (depInfo.isInputIndep()) {
        return depInfo;
    }
    return addOnDependencyInfo(depInfo);
}

DepInfo NonDeterministicBasicBlockAnaliser::addOnDependencyInfo(const DepInfo& info)
{
    auto newInfo = info;
    newInfo.mergeDependencies(m_nonDetDeps);
    return newInfo;
}

ValueDepInfo NonDeterministicBasicBlockAnaliser::addOnDependencyInfo(const ValueDepInfo& info)
{
    auto newInfo = info;
    ValueDepInfo nonDetDeps_info = newInfo;
    nonDetDeps_info.updateCompositeValueDep(m_nonDetDeps);
    newInfo.mergeDependencies(nonDetDeps_info);
    return newInfo;
}


} // namespace input_dependency

