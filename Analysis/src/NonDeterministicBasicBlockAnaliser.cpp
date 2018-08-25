#include "input-dependency/Analysis/NonDeterministicBasicBlockAnaliser.h"

#include "input-dependency/Analysis/IndirectCallSitesAnalysis.h"
#include "input-dependency/Analysis/Utils.h"

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
    for (const auto& value_dep : m_nonDetDeps.getValueDependencies()) {
        if (auto* global = llvm::dyn_cast<llvm::GlobalVariable>(value_dep)) {
            m_referencedGlobals.insert(global);
        }
    }
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
    for (auto& item : m_instructions) {
        if (item.second.isInputDep()) {
            m_dataDependentInstrs.insert(item.first);
        } else if (item.second.isInputArgumentDep()
                   && Utils::haveIntersection(dependentArgs, item.second.getArgumentDependencies())) {
            m_dataDependentInstrs.insert(item.first);
        }
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
    finalizeInstructions(globalsDeps, m_instructions);
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
    return m_dataDependentInstrs.find(I) != m_dataDependentInstrs.end();
}

bool NonDeterministicBasicBlockAnaliser::isDataDependent(llvm::Instruction* I, const ArgumentDependenciesMap& depArgs) const
{
    auto pos = m_instructions.find(I);
    if (pos == m_instructions.end()) {
        return false;
    }
    if (pos->second.isInputDep()) {
        return true;
    }
    return Utils::isInputDependentForArguments(pos->second, depArgs);
}

bool NonDeterministicBasicBlockAnaliser::isArgumentDependent(llvm::BasicBlock* block) const
{
    return m_nonDetDeps.isInputArgumentDep();
}

void NonDeterministicBasicBlockAnaliser::addControlDependencies(ValueDepInfo& valueDepInfo)
{
    valueDepInfo = addOnDependencyInfo(valueDepInfo);
}

void NonDeterministicBasicBlockAnaliser::addControlDependencies(DepInfo& depInfo)
{
    depInfo = addOnDependencyInfo(depInfo);
}

DepInfo NonDeterministicBasicBlockAnaliser::getInstructionDependencies(llvm::Instruction* instr)
{
    auto pos = m_instructions.find(instr);
    if (pos != m_instructions.end()) {
        return pos->second;
    }
    return BasicBlockAnalysisResult::getInstructionDependencies(instr);
}

ValueDepInfo NonDeterministicBasicBlockAnaliser::getValueDependencies(llvm::Value* value)
{
    auto pos = m_valueDataDependencies.find(value);
    if (pos != m_valueDataDependencies.end()) {
        return pos->second;
    }
    return  BasicBlockAnalysisResult::getValueDependencies(value);
}

ValueDepInfo NonDeterministicBasicBlockAnaliser::getCompositeValueDependencies(llvm::Value* value, llvm::Instruction* element_instr)
{
    auto pos = m_valueDataDependencies.find(value);
    if (pos == m_valueDataDependencies.end()) {
        return BasicBlockAnalysisResult::getCompositeValueDependencies(value, element_instr);
    }
    return pos->second.getValueDep(element_instr);
}

void NonDeterministicBasicBlockAnaliser::updateValueDependencies(llvm::Value* value, const DepInfo& info,
                                                                 bool update_aliases, int arg_idx)
{
    auto res = m_valueDataDependencies.insert(std::make_pair(value, ValueDepInfo(info)));
    bool input_indep = info.isInputIndep();
    if (!res.second) {
        input_indep &= res.first->second.isInputIndep();
        res.first->second.updateCompositeValueDep(info);
    }
    if (update_aliases) {
        updateAliasesDependencies(value, res.first->second, m_valueDataDependencies);
    }
    BasicBlockAnalysisResult::updateValueDependencies(value,
                                                      !input_indep ? addOnDependencyInfo(info) : info,
                                                      update_aliases, arg_idx);
}

void NonDeterministicBasicBlockAnaliser::updateValueDependencies(llvm::Value* value, const ValueDepInfo& info,
                                                                 bool update_aliases, int arg_idx)
{
    auto res = m_valueDataDependencies.insert(std::make_pair(value, info));
    bool input_indep = info.isInputIndep();
    if (!res.second) {
        input_indep &= res.first->second.isInputIndep();
        res.first->second.updateValueDep(info);
    }
    if (update_aliases) {
        updateAliasesDependencies(value, res.first->second, m_valueDataDependencies);
    }

    BasicBlockAnalysisResult::updateValueDependencies(value,
                                                      !input_indep ? addOnDependencyInfo(info) : info,
                                                      update_aliases, arg_idx);
}

void NonDeterministicBasicBlockAnaliser::updateCompositeValueDependencies(llvm::Value* value,
                                                                          llvm::Instruction* elInstr,
                                                                          const ValueDepInfo& info)
{
    auto res = m_valueDependencies.insert(std::make_pair(value, ValueDepInfo(info)));
    if (!res.second) {
        res.first->second.updateValueDep(elInstr, info);
    }
    updateAliasesDependencies(value, elInstr, res.first->second, m_valueDataDependencies);
    BasicBlockAnalysisResult::updateCompositeValueDependencies(value, elInstr, addOnDependencyInfo(info));
}

void NonDeterministicBasicBlockAnaliser::updateInstructionDependencies(llvm::Instruction* instr, const DepInfo& info)
{
    auto res = m_instructions.insert(std::make_pair(instr, info));
    if (!res.second) {
        res.first->second.mergeDependencies(info);
    }
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

