#include "input-dependency/Analysis/BasicBlockAnalysisResult.h"

#include "input-dependency/Analysis/Utils.h"
#include "input-dependency/Analysis/FunctionAnaliser.h"
#include "input-dependency/Analysis/InputDepConfig.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/Constants.h"
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

BasicBlockAnalysisResult::BasicBlockAnalysisResult(llvm::Function* F,
                                                   llvm::AAResults& AAR,
                                                   const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                                                   const IndirectCallSitesAnalysisResult& indirectCallsInfo,
                                                   const Arguments& inputs,
                                                   const FunctionAnalysisGetter& Fgetter,
                                                   llvm::BasicBlock* BB)
                                : DependencyAnaliser(F, AAR, virtualCallsInfo, indirectCallsInfo, inputs, Fgetter)
                                , m_BB(BB)
                                , m_is_inputDep(false)
{
}

DepInfo BasicBlockAnalysisResult::getBlockDependencies() const
{
    return DepInfo(DepInfo::INPUT_INDEP);
}

void BasicBlockAnalysisResult::gatherResults()
{
    analyze();
}

void BasicBlockAnalysisResult::finalizeResults(const ArgumentDependenciesMap& dependentArgs)
{
    finalize(dependentArgs);
}

void BasicBlockAnalysisResult::finalizeGlobals(const GlobalVariableDependencyMap& globalsDeps)
{
    finalize(globalsDeps);
}

void BasicBlockAnalysisResult::dumpResults() const
{
    llvm::dbgs() << "\nDump block " << m_BB->getName() << "\n";
    dump();
}

void BasicBlockAnalysisResult::analyze()
{
    //llvm::dbgs() << "Analise block " << m_BB->getName() << "\n";
    for (auto& I : *m_BB) {
        if (auto* allocInst = llvm::dyn_cast<llvm::AllocaInst>(&I)) {
            // collect alloca value with input dependency state INPUT_DEP
            m_valueDependencies.insert(std::make_pair(allocInst,
                        ValueDepInfo(allocInst->getAllocatedType(), DepInfo(DepInfo::INPUT_DEP))));
            // collect alloca instruction with input dependency state INPUT_INDEP
            updateInstructionDependencies(allocInst, DepInfo(DepInfo::INPUT_DEP));
        } else if (auto* retInst = llvm::dyn_cast<llvm::ReturnInst>(&I)) {
            processReturnInstr(retInst);
        }  else if (auto* branchInst = llvm::dyn_cast<llvm::BranchInst>(&I)) {
            processBranchInst(branchInst);
        } else if (auto* storeInst = llvm::dyn_cast<llvm::StoreInst>(&I)) {
            processStoreInst(storeInst);
        } else if (auto* callInst = llvm::dyn_cast<llvm::CallInst>(&I)) {
            processCallInst(callInst);
        } else if (auto* invokeInst = llvm::dyn_cast<llvm::InvokeInst>(&I)) {
            processInvokeInst(invokeInst);
        } else if (auto* phi = llvm::dyn_cast<llvm::PHINode>(&I)) {
            processPhiNode(phi);
        } else if (auto* bitcast = llvm::dyn_cast<llvm::BitCastInst>(&I)) {
            processBitCast(bitcast);
        } else if (auto* getElPtr = llvm::dyn_cast<llvm::GetElementPtrInst>(&I)) {
            processGetElementPtrInst(getElPtr);
        } else {
            processInstruction(&I);
        }
    }
}

void BasicBlockAnalysisResult::addControlDependencies(ValueDepInfo& valueDepInfo)
{
    // Nothing to do here
}

void BasicBlockAnalysisResult::addControlDependencies(DepInfo& depInfo)
{
    // Nothing to do here
}

DepInfo BasicBlockAnalysisResult::getInstructionDependencies(llvm::Instruction* instr)
{
    auto deppos = m_inputDependentInstrs.find(instr);
    if (deppos != m_inputDependentInstrs.end()) {
        return deppos->second;
    }
    auto indeppos = m_inputIndependentInstrs.find(instr);
    if (indeppos != m_inputIndependentInstrs.end()) {
        return DepInfo(DepInfo::INPUT_INDEP);
    }
    if (auto* loadInst = llvm::dyn_cast<llvm::LoadInst>(instr)) {
        return getLoadInstrDependencies(loadInst);
    }

    return determineInstructionDependenciesFromOperands(instr);
}

ValueDepInfo BasicBlockAnalysisResult::getValueDependencies(llvm::Value* value)
{
    auto pos = m_valueDependencies.find(value);
    if (pos != m_valueDependencies.end()) {
        return pos->second;
    }
    auto initial_val_pos = m_initialDependencies.find(value);
    if (initial_val_pos != m_initialDependencies.end()) {
        m_valueDependencies.insert(std::make_pair(value, initial_val_pos->second));
        return initial_val_pos->second;
    }
    return ValueDepInfo();
}

ValueDepInfo BasicBlockAnalysisResult::getCompositeValueDependencies(llvm::Value* value, llvm::Instruction* element_instr)
{
    ValueDepInfo valueDepInfo = getValueDependencyInfo(value);
    if (!valueDepInfo.isDefined()) {
        return ValueDepInfo();
    }
    return valueDepInfo.getValueDep(element_instr);
}

void BasicBlockAnalysisResult::updateInstructionDependencies(llvm::Instruction* instr, const DepInfo& info)
{
    switch (info.getDependency()) {
    case DepInfo::INPUT_DEP:
    case DepInfo::INPUT_ARGDEP:
    case DepInfo::VALUE_DEP:
        m_inputDependentInstrs[instr].mergeDependencies(info);
        break;
    case DepInfo::INPUT_INDEP:
        m_inputIndependentInstrs.insert(instr);
        break;
    default:
        assert(false);
    };
}

void BasicBlockAnalysisResult::updateValueDependencies(llvm::Value* value, const DepInfo& info,
                                                       bool update_aliases, int arg_idx)
{
    assert(info.isDefined());
    if (auto* global = llvm::dyn_cast<llvm::GlobalVariable>(value)) {
        m_referencedGlobals.insert(global);
        m_modifiedGlobals.insert(global);
    }
    auto res = m_valueDependencies.insert(std::make_pair(value, ValueDepInfo(value->getType(), info)));
    if (!res.second) {
        res.first->second.updateCompositeValueDep(info);
    }
    if (update_aliases) {
        updateAliasesDependencies(value, res.first->second, m_valueDependencies);
        updateAliasingOutArgDependencies(value, res.first->second, arg_idx);
    }
}

void BasicBlockAnalysisResult::updateValueDependencies(llvm::Value* value, const ValueDepInfo& info,
                                                       bool update_aliases, int arg_idx)
{
    assert(info.isDefined());
    if (auto* global = llvm::dyn_cast<llvm::GlobalVariable>(value)) {
        m_referencedGlobals.insert(global);
        m_modifiedGlobals.insert(global);
    }
    auto res = m_valueDependencies.insert(std::make_pair(value, info));
    if (!res.second) {
        res.first->second.updateValueDep(info);
    }
    if (update_aliases) {
        updateAliasesDependencies(value, res.first->second, m_valueDependencies);
        updateAliasingOutArgDependencies(value, res.first->second, arg_idx);
    }
}

void BasicBlockAnalysisResult::updateCompositeValueDependencies(llvm::Value* value,
                                                                llvm::Instruction* elInstr,
                                                                const ValueDepInfo& info)
{
    assert(info.isDefined());
    if (auto* global = llvm::dyn_cast<llvm::GlobalVariable>(value)) {
        m_referencedGlobals.insert(global);
        m_modifiedGlobals.insert(global);
    }
    auto res = m_valueDependencies.insert(std::make_pair(value, ValueDepInfo(info)));
    res.first->second.updateValueDep(elInstr, info);
    updateAliasesDependencies(value, elInstr, res.first->second, m_valueDependencies);
    updateAliasingOutArgDependencies(value, info);
}

void BasicBlockAnalysisResult::updateReturnValueDependencies(const ValueDepInfo& info)
{
    // bb can have only one return value, hence is safe to assign info
    m_returnValueDependencies.updateValueDep(info);
}

ValueDepInfo BasicBlockAnalysisResult::getRefInfo(llvm::Instruction* instr)
{
    ValueDepInfo info;
    const auto& DL = instr->getModule()->getDataLayout();
    for (const auto& dep : m_valueDependencies) {
        if (!dep.first->getType()->isSized()) {
            continue;
        }
        auto modRef = m_AAR.getModRefInfo(instr, dep.first, DL.getTypeStoreSize(dep.first->getType()));
        if (modRef == llvm::ModRefInfo::MustRef || modRef == llvm::ModRefInfo::Ref) {
            info.mergeDependencies(dep.second);
        }
    }
    return info;
}

void BasicBlockAnalysisResult::updateAliasesDependencies(llvm::Value* val, const ValueDepInfo& info, ValueDependencies& valueDependencies)
{
    //llvm::dbgs() << "updateAliasesDependencies1 " << *val << "\n";
    llvm::Instruction* value_instr = llvm::dyn_cast<llvm::Instruction>(val);
    for (auto& valDep : valueDependencies) {
        if (valDep.first == val) {
            continue;
        }
        auto alias = m_AAR.alias(val, valDep.first);
        // what about partial alias
        if (alias == llvm::AliasResult::MayAlias) {
            //llvm::dbgs() << "May aliases " << *valDep.first << "\n";
            value_instr ? valDep.second.mergeDependencies(value_instr, info)
                        : valDep.second.mergeDependencies(info);
        } else if (alias == llvm::AliasResult::MustAlias) {
            //llvm::dbgs() << "Must aliases " << *valDep.first << "\n";
            value_instr ? valDep.second.updateValueDep(value_instr, info)
                        : valDep.second.updateValueDep(info);
        }
    }
    for (auto& valDep : m_initialDependencies) {
        if (valueDependencies.find(valDep.first) != valueDependencies.end()) {
            continue;
        }
        if (valDep.first == val) {
            continue;
        }
        auto alias = m_AAR.alias(val, valDep.first);
        if (alias == llvm::AliasResult::MayAlias) {
            //llvm::dbgs() << "May aliases " << *valDep.first << "\n";
            value_instr ? valDep.second.mergeDependencies(value_instr, info)
                        : valDep.second.mergeDependencies(info);
        } else if (alias == llvm::AliasResult::MustAlias) {
            //llvm::dbgs() << "Must aliases " << *valDep.first << "\n";
            value_instr ? valDep.second.updateValueDep(value_instr, info)
                        : valDep.second.updateValueDep(info);
        }
    }
}

void BasicBlockAnalysisResult::updateAliasesDependencies(llvm::Value* val, llvm::Instruction* elInstr, const ValueDepInfo& info, ValueDependencies& valueDependencies)
{
    //llvm::dbgs() << "updateAliasesDependencies2 " << *val << "  " << *elInstr << "\n";
    for (auto& valDep : valueDependencies) {
        if (valDep.first == val) {
            continue;
        }
        auto alias = m_AAR.alias(val, valDep.first);
        if (alias == llvm::AliasResult::MayAlias || alias == llvm::AliasResult::PartialAlias) {
            //llvm::dbgs() << "May aliases " << *valDep.first << "\n";
            valDep.second.mergeDependencies(elInstr, info);
        } else if (alias == llvm::AliasResult::MustAlias) {
            //llvm::dbgs() << "Must aliases " << *valDep.first << "\n";
            valDep.second.updateValueDep(elInstr, info);
        }
    }
    for (auto& valDep : m_initialDependencies) {
        if (valueDependencies.find(valDep.first) != valueDependencies.end()) {
            continue;
        }
        if (valDep.first == val) {
            continue;
        }
        auto alias = m_AAR.alias(val, valDep.first);
        if (alias == llvm::AliasResult::MayAlias || alias == llvm::AliasResult::PartialAlias) {
            valDep.second.mergeDependencies(elInstr, info);
        } else if (alias == llvm::AliasResult::MustAlias) {
            valDep.second.updateValueDep(elInstr, info);
        }
    }
}

void BasicBlockAnalysisResult::updateAliasingOutArgDependencies(llvm::Value* value, const ValueDepInfo& info, int arg_idx)
{
    llvm::Instruction* value_instr = llvm::dyn_cast<llvm::Instruction>(value);
    for (auto& arg : m_outArgDependencies) {
        if (arg_idx != -1 && arg_idx != arg.first->getArgNo()) {
            continue;
        }
        auto alias = m_AAR.alias(value, arg.first);
        if (alias != llvm::AliasResult::NoAlias) {
            //llvm::dbgs() << "   May alias\n";
            if (alias == llvm::AliasResult::MayAlias || alias == llvm::AliasResult::PartialAlias) {
                value_instr ? arg.second.mergeDependencies(value_instr, info)
                            : arg.second.mergeDependencies(info);
            } else if (alias == llvm::AliasResult::MustAlias) {
                //llvm::dbgs() << "   Must alias\n";
                value_instr ? arg.second.updateValueDep(value_instr, info)
                            : arg.second.updateValueDep(info);
            }
        }
    }
}

void BasicBlockAnalysisResult::updateModAliasesDependencies(llvm::StoreInst* storeInst, const ValueDepInfo& info)
{
    const auto& DL = storeInst->getModule()->getDataLayout();
    for (auto& dep : m_valueDependencies) {
        if (!dep.first->getType()->isSized()) {
            continue;
        }
        auto modRef = m_AAR.getModRefInfo(storeInst, dep.first, DL.getTypeStoreSize(dep.first->getType()));
        if (modRef == llvm::ModRefInfo::MustMod || modRef == llvm::ModRefInfo::Mod) {
            // if modifies given value should modify other aliases too, thus no need to set update_aliases flag
            updateValueDependencies(dep.first, info, false);
        }
    }
    for (auto& dep : m_initialDependencies) {
        if (m_valueDependencies.find(dep.first) != m_valueDependencies.end()) {
            continue;
        }
        if (!dep.first->getType()->isSized()) {
            continue;
        }
        auto modRef = m_AAR.getModRefInfo(storeInst, dep.first, DL.getTypeStoreSize(dep.first->getType()));
        if (modRef == llvm::ModRefInfo::MustMod || modRef == llvm::ModRefInfo::Mod) {
            updateValueDependencies(dep.first, info, false);
        }
    }
}

void BasicBlockAnalysisResult::updateRefAliasesDependencies(llvm::Instruction* instr, const ValueDepInfo& info)
{
    const auto& DL = instr->getModule()->getDataLayout();
    for (auto& dep : m_valueDependencies) {
        if (!dep.first->getType()->isSized()) {
            continue;
        }
        auto modRef = m_AAR.getModRefInfo(instr, dep.first, DL.getTypeStoreSize(dep.first->getType()));
        if (modRef == llvm::ModRefInfo::MustRef || modRef == llvm::ModRefInfo::Ref) {
            updateValueDependencies(dep.first, info, false);
        }
        auto alias = m_AAR.alias(instr, dep.first);
        if (alias == llvm::AliasResult::NoAlias) {
            continue;
        } else {
            updateValueDependencies(dep.first, info, false);
        }
    }
}

void BasicBlockAnalysisResult::markFunctionsForValue(llvm::Value* value)
{
    auto functions_pos = m_functionValues.find(value);
    if (functions_pos == m_functionValues.end()) {
        return;
    }
    auto& functions = functions_pos->second;
    for (auto& F : functions) {
        auto FA = m_FAG(F);
        // if no FA save for later point?
        llvm::dbgs() << "Set input dependency of a function " << F->getName() << "\n";
        if (FA) {
            FA->setIsInputDepFunction(true);
        }
        auto pos = m_functionCallInfo.insert(std::make_pair(F, FunctionCallDepInfo(*F)));
        pos.first->second.setIsCallback(true);
        m_calledFunctions.insert(F);
        InputDepConfig::get().add_input_dep_function(F);
    }
}

void BasicBlockAnalysisResult::markCallbackFunctionsForValue(llvm::Value* value)
{
    for (auto& valDep : m_valueDependencies) {
        if (valDep.first == value) {
            markFunctionsForValue(value);
        }
        auto alias = m_AAR.alias(value, valDep.first);
        // what about partial alias
        if (alias == llvm::AliasResult::MayAlias || alias == llvm::AliasResult::MustAlias) {
            markFunctionsForValue(valDep.first);
            //llvm::dbgs() << "May aliases " << *valDep.first << "\n";
        }
    }
    for (auto& valDep : m_initialDependencies) {
        if (m_valueDependencies.find(valDep.first) != m_valueDependencies.end()) {
            continue;
        }
        if (valDep.first == value) {
            markFunctionsForValue(value);
        }
        auto alias = m_AAR.alias(value, valDep.first);
        // what about partial alias
        if (alias == llvm::AliasResult::MayAlias || alias == llvm::AliasResult::MustAlias) {
            markFunctionsForValue(valDep.first);
            //llvm::dbgs() << "May aliases " << *valDep.first << "\n";
        }
    }
}

void BasicBlockAnalysisResult::removeCallbackFunctionsForValue(llvm::Value* value)
{
    for (auto& valDep : m_valueDependencies) {
        auto pos = m_functionValues.find(valDep.first);
        if (pos == m_functionValues.end()) {
            continue;
        }
        if (valDep.first == value) {
            m_functionValues.erase(pos);
            continue;
        }
        auto alias = m_AAR.alias(value, valDep.first);
        // must alias only, as in case of structs a callback field "may alias" even with other fields
        if (/*alias == llvm::AliasResult::MayAlias || */alias == llvm::AliasResult::MustAlias) {
            m_functionValues.erase(pos);
            //llvm::dbgs() << "May aliases " << *valDep.first << "\n";
        }
    }
    for (auto& valDep : m_initialDependencies) {
        if (m_valueDependencies.find(valDep.first) != m_valueDependencies.end()) {
            continue;
        }
        auto pos = m_functionValues.find(valDep.first);
        if (pos == m_functionValues.end()) {
            continue;
        }
        if (valDep.first == value) {
            m_functionValues.erase(pos);
            continue;
        }
        auto alias = m_AAR.alias(value, valDep.first);
        // what about partial alias
        if (/*alias == llvm::AliasResult::MayAlias || */alias == llvm::AliasResult::MustAlias) {
            m_functionValues.erase(pos);
            //llvm::dbgs() << "May aliases " << *valDep.first << "\n";
        }
    }
}

void BasicBlockAnalysisResult::setInitialValueDependencies(
                    const ValueDependencies& valueDependencies)
{
    m_initialDependencies = valueDependencies;
}

void BasicBlockAnalysisResult::setOutArguments(const ArgumentDependenciesMap& outArgs)
{
    m_outArgDependencies = outArgs;
}

void BasicBlockAnalysisResult::setCallbackFunctions(const ValueCallbackMap& callbacks)
{
    m_functionValues = callbacks;
}

bool BasicBlockAnalysisResult::isInputDependent(llvm::BasicBlock* block) const
{
    assert(block == m_BB);
    return m_is_inputDep;
}

bool BasicBlockAnalysisResult::isInputDependent(llvm::BasicBlock* block,
                                                const ArgumentDependenciesMap& depArgs) const
{
    // if this function is called means block is neither argument dep (nonDetBlock would have been called) nor input
    // dependent block;
    return false;
}

bool BasicBlockAnalysisResult::isInputDependent(llvm::Instruction* instr) const
{
    assert(instr->getParent() == m_BB);
    if (m_finalized) {
        return m_finalInputDependentInstrs.find(instr) != m_finalInputDependentInstrs.end();
    }
    return m_inputDependentInstrs.find(instr) != m_inputDependentInstrs.end();
}

bool BasicBlockAnalysisResult::isInputDependent(llvm::Instruction* instr,
                                                const ArgumentDependenciesMap& depArgs) const
{
    auto pos = m_inputDependentInstrs.find(instr);
    if (pos == m_inputDependentInstrs.end()) {
        // if is not in non-final input dep instructions set, means is input independent
        return false;
    }
    const auto& deps = pos->second;
    if (deps.isInputDep()) {
        // is input dep
        return true;
    }
    // if got to this point means is input arg dep, as all args are input indep - return false
    if (depArgs.empty()) {
        return false;
    }
    return (deps.isInputArgumentDep() && Utils::haveIntersection(depArgs, deps.getArgumentDependencies()));
}

bool BasicBlockAnalysisResult::isInputIndependent(llvm::Instruction* instr) const
{
    assert(instr->getParent()->getParent() == m_F);
    return m_inputIndependentInstrs.find(instr) != m_inputIndependentInstrs.end() && !isInputDependent(instr);
}

bool BasicBlockAnalysisResult::isControlDependent(llvm::Instruction* I) const
{
    return m_is_inputDep;
}

bool BasicBlockAnalysisResult::isDataDependent(llvm::Instruction* I) const
{
    // TODO: check correctness of this statement
    return !m_is_inputDep && isInputDependent(I);
}

bool BasicBlockAnalysisResult::isDataDependent(llvm::Instruction* I, const ArgumentDependenciesMap& depArgs) const
{
    return !isInputDependent(m_BB, depArgs) && isInputDependent(I, depArgs);
}

bool BasicBlockAnalysisResult::isArgumentDependent(llvm::Instruction* I) const
{
    auto pos = m_inputDependentInstrs.find(I);
    if (pos == m_inputDependentInstrs.end()) {
        return false;
    }
    return pos->second.isInputArgumentDep();
}

bool BasicBlockAnalysisResult::isArgumentDependent(llvm::BasicBlock* block) const
{
    return false;
}

bool BasicBlockAnalysisResult::isGlobalDependent(llvm::Instruction* I) const
{
    return m_globalDependentInstrs.find(I) != m_globalDependentInstrs.end();
}

bool BasicBlockAnalysisResult::isInputIndependent(llvm::Instruction* instr,
                                                  const ArgumentDependenciesMap& depArgs) const
{
    auto pos = m_inputDependentInstrs.find(instr);
    if (pos == m_inputDependentInstrs.end()) {
        return true;
    }
    const auto& deps = pos->second;
    if (deps.isInputDep()) {
        return false;
    }
    return deps.isInputIndep()
        || depArgs.empty()
        || (deps.isInputArgumentDep() && !Utils::haveIntersection(depArgs, deps.getArgumentDependencies()));
}

bool BasicBlockAnalysisResult::hasValueDependencyInfo(llvm::Value* val) const
{
    auto pos = m_valueDependencies.find(val);
    if (pos != m_valueDependencies.end()) {
        return true;
    }
    return m_initialDependencies.find(val) != m_initialDependencies.end();
}

ValueDepInfo BasicBlockAnalysisResult::getValueDependencyInfo(llvm::Value* val)
{
    auto pos = m_valueDependencies.find(val);
    if (pos != m_valueDependencies.end()) {
        return pos->second;
    }
    auto initial_val_pos = m_initialDependencies.find(val);
    // This is from external usage, through DependencyAnalysisResult interface
    if (initial_val_pos == m_initialDependencies.end()) {
        return ValueDepInfo();
    }
    assert(initial_val_pos != m_initialDependencies.end());
    // add referenced value
    const auto& info = initial_val_pos->second;
    m_valueDependencies.insert(std::make_pair(val, info));
    return info;
}

DepInfo BasicBlockAnalysisResult::getInstructionDependencies(llvm::Instruction* instr) const
{
    auto pos = m_inputDependentInstrs.find(instr);
    if (pos == m_inputDependentInstrs.end()) {
        return DepInfo(DepInfo::INPUT_INDEP);
    }
    return pos->second;
}

const BasicBlockAnalysisResult::ValueDependencies& BasicBlockAnalysisResult::getValuesDependencies() const
{
    return m_valueDependencies;
}

const BasicBlockAnalysisResult::ValueDependencies& BasicBlockAnalysisResult::getInitialValuesDependencies() const
{
    return m_initialDependencies;
}

const ValueDepInfo& BasicBlockAnalysisResult::getReturnValueDependencies() const
{
    return m_returnValueDependencies;
}

const BasicBlockAnalysisResult::ArgumentDependenciesMap&
BasicBlockAnalysisResult::getOutParamsDependencies() const
{
    return m_outArgDependencies;
}

const BasicBlockAnalysisResult::ValueCallbackMap&
BasicBlockAnalysisResult::getCallbackFunctions() const
{
    return m_functionValues;
}

const DependencyAnaliser::FunctionCallsArgumentDependencies&
BasicBlockAnalysisResult::getFunctionsCallInfo() const
{
    return m_functionCallInfo;
}

const FunctionCallDepInfo& BasicBlockAnalysisResult::getFunctionCallInfo(llvm::Function* F) const
{
    auto pos = m_functionCallInfo.find(F);
    assert(pos != m_functionCallInfo.end());
    return pos->second;
}

bool BasicBlockAnalysisResult::changeFunctionCall(llvm::Instruction* instr, llvm::Function* oldF, llvm::Function* newCallee)
{
    //llvm::dbgs() << "   Change called fuction in instruction " << *instr << " to " << newCallee->getName() << "\n";

    if (auto call = llvm::dyn_cast<llvm::CallInst>(instr)) {
        call->setCalledFunction(newCallee);
    } else if (auto invoke = llvm::dyn_cast<llvm::InvokeInst>(instr)) {
        invoke->setCalledFunction(newCallee);
    } else {
        assert(false);
    }
    auto callDepInfo_pos = m_functionCallInfo.find(oldF);
    if (callDepInfo_pos == m_functionCallInfo.end()) {
        //llvm::dbgs() << "No call of function " << oldF->getName() << " in block " << m_BB->getName() << "\n";
        return false;
    }
    auto& callDepInfo = callDepInfo_pos->second;
    const FunctionCallDepInfo::ArgumentDependenciesMap&  calledArgDepMap = callDepInfo.getArgumentsDependencies(instr);
    const FunctionCallDepInfo::GlobalVariableDependencyMap& globalsDeps = callDepInfo.getGlobalsDependencies(instr);
    FunctionCallDepInfo newCallDepInfo(*newCallee);
    newCallDepInfo.addCall(instr, calledArgDepMap);
    newCallDepInfo.addCall(instr, globalsDeps);
    auto insert_res = m_functionCallInfo.insert(std::make_pair(newCallee, newCallDepInfo));
    if (!insert_res.second) {
        insert_res.first->second.addDepInfo(newCallDepInfo);
    }
    callDepInfo.removeCall(instr);
    if (callDepInfo.empty()) {
        m_functionCallInfo.erase(callDepInfo_pos);
    }
    m_calledFunctions.insert(newCallee);
    if (!hasFunctionCallInfo(oldF)) {
        m_calledFunctions.erase(oldF);
    }
    //llvm::dbgs() << "   Result of called function change " << *instr << "\n";
    return true;
}

bool BasicBlockAnalysisResult::hasFunctionCallInfo(llvm::Function* F) const
{
    return m_functionCallInfo.find(F) != m_functionCallInfo.end();
}

const FunctionSet& BasicBlockAnalysisResult::getCallSitesData() const
{
    return m_calledFunctions;
}

const GlobalsSet& BasicBlockAnalysisResult::getReferencedGlobals() const
{
    return m_referencedGlobals;
}

const GlobalsSet& BasicBlockAnalysisResult::getModifiedGlobals() const
{
    return m_modifiedGlobals;
}

long unsigned BasicBlockAnalysisResult::get_input_dep_blocks_count() const
{
    return m_is_inputDep ? 1 : 0;
}

long unsigned BasicBlockAnalysisResult::get_input_indep_blocks_count() const
{
    return m_is_inputDep ? 0 : 1;
}

long unsigned BasicBlockAnalysisResult::get_input_dep_count() const
{
    return m_finalInputDependentInstrs.size();
}

long unsigned BasicBlockAnalysisResult::get_input_indep_count() const
{
    return m_inputIndependentInstrs.size();
}

long unsigned BasicBlockAnalysisResult::get_data_indep_count() const
{
    // TODO: think about caching
    long unsigned count = 0;
    for (auto& I : *m_BB) {
        if (!isDataDependent(&I)) {
            ++count;
        }
    }
    return count;
}

long unsigned BasicBlockAnalysisResult::get_input_unknowns_count() const
{
    assert(m_BB->getInstList().size() >= get_input_dep_count() + get_input_indep_count());
    auto count = m_BB->getInstList().size() - get_input_dep_count() - get_input_indep_count();
    return count;
}

DepInfo BasicBlockAnalysisResult::getLoadInstrDependencies(llvm::LoadInst* instr)
{
    auto* loadOp = instr->getPointerOperand();
    DepInfo instrDepInfo;
    ValueDepInfo valueDepInfo = getValueDependencies(loadOp);
    if (valueDepInfo.isDefined()) {
        updateValueDependencies(instr, valueDepInfo, false);
        return valueDepInfo.getValueDep();
    }
    if (auto opinstr = llvm::dyn_cast<llvm::Instruction>(loadOp)) {
        if (!llvm::dyn_cast<llvm::AllocaInst>(opinstr)) {
            instrDepInfo = getInstructionDependencies(opinstr);
            if (instrDepInfo.isDefined()) {
                updateValueDependencies(instr, instrDepInfo, false);
                return instrDepInfo;
            }
        }
    }
    if (auto constExpr = llvm::dyn_cast<llvm::ConstantExpr>(loadOp)) {
        auto* opinstr = constExpr->getAsInstruction();
        if (opinstr) {
            instrDepInfo = getInstructionDependencies(opinstr);
            opinstr->deleteValue();
            if (instrDepInfo.isDefined()) {
                updateValueDependencies(instr, instrDepInfo, false);
                return instrDepInfo;
            }
        }
    }

    valueDepInfo = getRefInfo(instr);
    if (valueDepInfo.isDefined()) {
        updateValueDependencies(instr, valueDepInfo, false);
        return valueDepInfo.getValueDep();
    }

    llvm::Value* loadedValue = getMemoryValue(loadOp);
    if (loadedValue == nullptr) {
        if (llvm::dyn_cast<llvm::Constant>(loadOp)) {
            updateValueDependencies(instr, DepInfo(DepInfo::INPUT_INDEP), false);
            return DepInfo(DepInfo::INPUT_INDEP);
        }
        instrDepInfo = getInstructionDependencies(llvm::dyn_cast<llvm::Instruction>(loadOp));
        updateValueDependencies(instr, instrDepInfo, false);
        return instrDepInfo;
    } else {
        auto args = isInput(loadedValue);
        if (!args.empty()) {
            updateValueDependencies(instr, DepInfo(DepInfo::INPUT_ARGDEP, args), false);
            return DepInfo(DepInfo::INPUT_ARGDEP, args);
        }
    }
    valueDepInfo = getValueDependencies(loadedValue);
    if (!valueDepInfo.isDefined()) {
        // might be unnecessary
        if (auto loadedValInstr = llvm::dyn_cast<llvm::Instruction>(loadedValue)) {
            instrDepInfo =  getInstructionDependencies(loadedValInstr);
            updateValueDependencies(instr, instrDepInfo, false);
            return instrDepInfo;
        }
        auto globalVal = llvm::dyn_cast<llvm::GlobalVariable>(loadedValue);
        assert(globalVal != nullptr);
        m_referencedGlobals.insert(globalVal);
        updateValueDependencies(instr, DepInfo(DepInfo::VALUE_DEP, ValueSet{globalVal}), false);
        return DepInfo(DepInfo::VALUE_DEP, ValueSet{globalVal});
    }
    assert(valueDepInfo.isDefined());
    updateValueDependencies(instr, valueDepInfo, false);
    return valueDepInfo.getValueDep();
}

DepInfo BasicBlockAnalysisResult::determineInstructionDependenciesFromOperands(llvm::Instruction* instr)
{
    DepInfo deps(DepInfo::INPUT_INDEP);
    for (auto op = instr->op_begin(); op != instr->op_end(); ++op) {
        if (auto* opInst = llvm::dyn_cast<llvm::Instruction>(op)) {
            const auto& value_dep = getValueDependencies(opInst);
            if (value_dep.isDefined()) {
                deps.mergeDependencies(value_dep.getValueDep());
            }
            const auto& c_deps = getInstructionDependencies(opInst);
            if (c_deps.isDefined()) {
                deps.mergeDependencies(c_deps);
            }
        } else if (auto* opVal = llvm::dyn_cast<llvm::Value>(op)) {
            if (auto* global = llvm::dyn_cast<llvm::GlobalVariable>(opVal)) {
                m_referencedGlobals.insert(global);
                deps.mergeDependencies(DepInfo(DepInfo::VALUE_DEP, ValueSet{global}));
            }
            auto c_args = isInput(opVal);
            if (!c_args.empty()) {
                deps.mergeDependencies(DepInfo(DepInfo::INPUT_ARGDEP, c_args));
            } else {
                const auto& valDeps = getValueDependencies(opVal);
                if (!valDeps.isDefined()) {
                    continue;
                }
                deps.mergeDependencies(valDeps.getValueDep());
            }
        }
    }
    return deps;
}

} // namespace input_dependency

