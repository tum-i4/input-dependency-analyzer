#include "input-dependency/Analysis/NonDeterministicReflectingBasicBlockAnaliser.h"

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
{
    for (const auto& value_dep : m_nonDeterministicDeps.getValueDependencies()) {
        if (auto* global = llvm::dyn_cast<llvm::GlobalVariable>(value_dep)) {
            m_referencedGlobals.insert(global);
        }
    }
}

DepInfo NonDeterministicReflectingBasicBlockAnaliser::getBlockDependencies() const
{
    return m_nonDeterministicDeps;
}

void NonDeterministicReflectingBasicBlockAnaliser::finalizeResults(const ArgumentDependenciesMap& dependentArgs)
{
    ReflectingBasicBlockAnaliser::finalizeResults(dependentArgs);
    if (m_nonDeterministicDeps.isInputDep()) {
        m_is_inputDep = true;
    }
    if (m_nonDeterministicDeps.isInputArgumentDep() && Utils::haveIntersection(dependentArgs, m_nonDeterministicDeps.getArgumentDependencies())) {
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

void NonDeterministicReflectingBasicBlockAnaliser::finalizeGlobals(const GlobalVariableDependencyMap& globalsDeps)
{
    BasicBlockAnalysisResult::finalizeGlobals(globalsDeps);
    if (!m_nonDeterministicDeps.isValueDep() && m_nonDeterministicDeps.getValueDependencies().empty()) {
        return;
    }
    finalizeValueDependencies(globalsDeps, m_nonDeterministicDeps);
    m_is_inputDep |= m_nonDeterministicDeps.isInputDep();
    finalizeInstructions(globalsDeps, m_instructions);
}

bool NonDeterministicReflectingBasicBlockAnaliser::isInputDependent(llvm::BasicBlock* block,
                                                                    const DependencyAnaliser::ArgumentDependenciesMap& depArgs) const
{
    assert(block == m_BB);
    if (m_nonDeterministicDeps.isInputDep() && m_nonDeterministicDeps.getArgumentDependencies().empty()) {
        return true;
    }
    if (depArgs.empty()) {
        return false;
    }
    return Utils::isInputDependentForArguments(m_nonDeterministicDeps, depArgs);
}

bool NonDeterministicReflectingBasicBlockAnaliser::isDataDependent(llvm::Instruction* I) const
{
    return m_dataDependentInstrs.find(I) != m_dataDependentInstrs.end();
}

bool NonDeterministicReflectingBasicBlockAnaliser::isDataDependent(llvm::Instruction* I, const ArgumentDependenciesMap& depArgs) const
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

bool NonDeterministicReflectingBasicBlockAnaliser::isArgumentDependent(llvm::BasicBlock* block) const
{
    return m_nonDeterministicDeps.isInputArgumentDep();
}

void NonDeterministicReflectingBasicBlockAnaliser::reflect(const DependencyAnaliser::ValueDependencies& dependencies,
                                                           const DepInfo& mandatory_deps)
{
    ReflectingBasicBlockAnaliser::reflect(dependencies, mandatory_deps);
    if (!m_nonDeterministicDeps.isValueDep()) {
        return;
    }
    std::vector<llvm::Value*> values_to_erase;
    const auto block_dependencies = m_nonDeterministicDeps.getValueDependencies();
    for (const auto dep : block_dependencies) {
        auto value_deps = m_valueDependencies.find(dep);
        if (value_deps == m_valueDependencies.end()) {
            continue;
        }
        m_nonDeterministicDeps.mergeDependencies(value_deps->second.getValueDep());
        values_to_erase.push_back(dep);
    }
    std::for_each(values_to_erase.begin(), values_to_erase.end(),
                  [this] (llvm::Value* val) { this->m_nonDeterministicDeps.getValueDependencies().erase(val); });
    for (auto& instr_dep : m_instructions) {
        values_to_erase.clear();
        const auto dependencies = instr_dep.second.getValueDependencies();
        for (const auto dep : dependencies) {
            // TODO: should this be m_valueDependencies?
            auto value_deps = m_valueDependencies.find(dep);
            if (value_deps == m_valueDependencies.end()) {
                continue;
            }
            instr_dep.second.mergeDependencies(value_deps->second.getValueDep());
            values_to_erase.push_back(dep);
        }
        std::for_each(values_to_erase.begin(), values_to_erase.end(),
                [&instr_dep] (llvm::Value* val) { instr_dep.second.getValueDependencies().erase(val); });
    }
}

void NonDeterministicReflectingBasicBlockAnaliser::addControlDependencies(ValueDepInfo& valueDepInfo)
{
    valueDepInfo = addOnDependencyInfo(valueDepInfo);
}

void NonDeterministicReflectingBasicBlockAnaliser::addControlDependencies(DepInfo& depInfo)
{
    depInfo = addOnDependencyInfo(depInfo);
}

DepInfo NonDeterministicReflectingBasicBlockAnaliser::getInstructionDependencies(llvm::Instruction* instr)
{
    auto pos = m_instructions.find(instr);
    if (pos != m_instructions.end()) {
        return pos->second;
    }

    return ReflectingBasicBlockAnaliser::getInstructionDependencies(instr);
}

ValueDepInfo NonDeterministicReflectingBasicBlockAnaliser::getValueDependencies(llvm::Value* value)
{
    auto pos = m_valueDataDependencies.find(value);
    if (pos != m_valueDataDependencies.end()) {
        return pos->second;
    }

    return ReflectingBasicBlockAnaliser::getValueDependencies(value);
}

void NonDeterministicReflectingBasicBlockAnaliser::updateValueDependencies(llvm::Value* value, const DepInfo& info,
                                                                           bool update_aliases, int arg_idx)
{
    auto res = m_valueDataDependencies.insert(std::make_pair(value, ValueDepInfo(info)));
    if (!res.second) {
        res.first->second.updateCompositeValueDep(info);
    }
    if (update_aliases) {
        updateAliasesDependencies(value, res.first->second, m_valueDataDependencies);
    }
    ReflectingBasicBlockAnaliser::updateValueDependencies(value, info, update_aliases, arg_idx);
}

void NonDeterministicReflectingBasicBlockAnaliser::updateValueDependencies(llvm::Value* value, const ValueDepInfo& info,
                                                                           bool update_aliases, int arg_idx)
{
    auto res = m_valueDataDependencies.insert(std::make_pair(value, info));
    if (!res.second) {
        res.first->second.updateValueDep(info);
    }
    if (update_aliases) {
        updateAliasesDependencies(value, res.first->second, m_valueDataDependencies);
    }
    ReflectingBasicBlockAnaliser::updateValueDependencies(value, info, update_aliases, arg_idx);
}

void NonDeterministicReflectingBasicBlockAnaliser::updateCompositeValueDependencies(llvm::Value* value,
                                                                                    llvm::Instruction* elInstr,
                                                                                    const ValueDepInfo& info)
{
    auto res = m_valueDependencies.insert(std::make_pair(value, ValueDepInfo(info)));
    if (!res.second) {
        res.first->second.updateValueDep(elInstr, info);
    }
    updateAliasesDependencies(value, elInstr, res.first->second, m_valueDataDependencies);
    ReflectingBasicBlockAnaliser::updateCompositeValueDependencies(value, elInstr, addOnDependencyInfo(info));
}


void NonDeterministicReflectingBasicBlockAnaliser::updateInstructionDependencies(llvm::Instruction* instr, const DepInfo& info)
{
    auto res = m_instructions.insert(std::make_pair(instr, info));
    if (!res.second) {
        res.first->second.mergeDependencies(info);
    }
    ReflectingBasicBlockAnaliser::updateInstructionDependencies(instr, addOnDependencyInfo(info));
}

void NonDeterministicReflectingBasicBlockAnaliser::updateReturnValueDependencies(const ValueDepInfo& info)
{
    ReflectingBasicBlockAnaliser::updateReturnValueDependencies(addOnDependencyInfo(info));
}

ValueDepInfo NonDeterministicReflectingBasicBlockAnaliser::getArgumentValueDependecnies(llvm::Value* argVal)
{
    auto depInfo = ReflectingBasicBlockAnaliser::getArgumentValueDependecnies(argVal);
    if (depInfo.isInputIndep()) {
        return depInfo;
    }
    return addOnDependencyInfo(depInfo);
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

