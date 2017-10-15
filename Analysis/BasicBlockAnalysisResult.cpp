#include "BasicBlockAnalysisResult.h"

#include "Utils.h"

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

void BasicBlockAnalysisResult::gatherResults()
{
    analize();
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

void BasicBlockAnalysisResult::analize()
{
    //llvm::dbgs() << "Analise block " << m_BB->getName() << "\n";
    for (auto& I : *m_BB) {
        //llvm::dbgs() << "Instruction " << I << "\n";
        if (auto* allocInst = llvm::dyn_cast<llvm::AllocaInst>(&I)) {
            // Note alloc instructions are at the begining of the function
            // Here just collect them with input indep state
            m_valueDependencies.insert(std::make_pair(allocInst, ValueDepInfo(allocInst)));
            updateInstructionDependencies(allocInst, DepInfo(DepInfo::INPUT_INDEP));
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

ValueDepInfo BasicBlockAnalysisResult:: getValueDependencies(llvm::Value* value)
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
    const auto& elDeps = valueDepInfo.getValueDep(element_instr);
    updateValueDependencies(value, valueDepInfo);
    return elDeps;
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

void BasicBlockAnalysisResult::updateValueDependencies(llvm::Value* value, const DepInfo& info)
{
    assert(info.isDefined());
    auto res = m_valueDependencies.insert(std::make_pair(value, ValueDepInfo(info)));
    if (!res.second) {
        res.first->second.updateCompositeValueDep(info);
    }
    if (!llvm::dyn_cast<llvm::GetElementPtrInst>(value)) {
        updateAliasesDependencies(value, res.first->second);
        updateAliasingOutArgDependencies(value, res.first->second);
    }
}

void BasicBlockAnalysisResult::updateValueDependencies(llvm::Value* value, const ValueDepInfo& info)
{
    assert(info.isDefined());
    auto res = m_valueDependencies.insert(std::make_pair(value, info));
    if (!res.second) {
        res.first->second.updateValueDep(info);
    }
    if (!llvm::dyn_cast<llvm::GetElementPtrInst>(value)) {
        updateAliasesDependencies(value, res.first->second);
        updateAliasingOutArgDependencies(value, res.first->second);
    }
}

void BasicBlockAnalysisResult::updateCompositeValueDependencies(llvm::Value* value,
                                                                llvm::Instruction* elInstr,
                                                                const ValueDepInfo& info)
{
    assert(info.isDefined());
    auto res = m_valueDependencies.insert(std::make_pair(value, ValueDepInfo(info)));
    res.first->second.updateValueDep(elInstr, info);
    updateAliasesDependencies(value, res.first->second);
    updateAliasingOutArgDependencies(value, info);
}

void BasicBlockAnalysisResult::updateReturnValueDependencies(const ValueDepInfo& info)
{
    m_returnValueDependencies.mergeDependencies(info);
}

DepInfo BasicBlockAnalysisResult::getDependenciesFromAliases(llvm::Value* val)
{
    DepInfo info;
    for (const auto& dep : m_valueDependencies) {
        auto alias = m_AAR.alias(val, dep.first);
        if (alias != llvm::AliasResult::NoAlias) {
            info.mergeDependencies(dep.second.getValueDep());
        }
    }
    return info;
}

DepInfo BasicBlockAnalysisResult::getRefInfo(llvm::LoadInst* loadInst)
{
    DepInfo info;
    //llvm::dbgs() << *loadInst << "\n";
    const auto& DL = loadInst->getModule()->getDataLayout();
    for (const auto& dep : m_valueDependencies) {
        auto modRef = m_AAR.getModRefInfo(loadInst, dep.first, DL.getTypeStoreSize(dep.first->getType()));
        if (modRef == llvm::ModRefInfo::MRI_Ref) {
            info.mergeDependencies(dep.second.getValueDep());
        }
    }
    return info;
}

void BasicBlockAnalysisResult::updateAliasesDependencies(llvm::Value* val, const ValueDepInfo& info)
{
    llvm::Instruction* instr = llvm::dyn_cast<llvm::Instruction>(val);
    for (auto& valDep : m_valueDependencies) {
        auto alias = m_AAR.alias(val, valDep.first);
        if (alias != llvm::AliasResult::NoAlias) {
            valDep.second.updateValueDep(info);
        }
    }
    for (auto& valDep : m_valueDependencies) {
        auto alias = m_AAR.alias(val, valDep.first);
        if (alias != llvm::AliasResult::NoAlias) {
            valDep.second.updateValueDep(info);
        }
    }
    for (auto& valDep : m_initialDependencies) {
        if (m_valueDependencies.find(valDep.first) != m_valueDependencies.end()) {
            continue;
        }
        auto alias = m_AAR.alias(val, valDep.first);
        if (alias != llvm::AliasResult::NoAlias) {
            m_valueDependencies.insert(std::make_pair(valDep.first, info));
        }
    }
}

void BasicBlockAnalysisResult::updateAliasingOutArgDependencies(llvm::Value* value, const ValueDepInfo& info)
{
    for (auto& arg : m_outArgDependencies) {
        auto alias = m_AAR.alias(value, arg.first);
        if (alias != llvm::AliasResult::NoAlias) {
            arg.second.updateValueDep(info);
        }
    }
}

void BasicBlockAnalysisResult::updateModAliasesDependencies(llvm::StoreInst* storeInst, const ValueDepInfo& info)
{
    const auto& DL = storeInst->getModule()->getDataLayout();
    for (auto& dep : m_valueDependencies) {
        auto modRef = m_AAR.getModRefInfo(storeInst, dep.first, DL.getTypeStoreSize(dep.first->getType()));
        if (modRef == llvm::ModRefInfo::MRI_Mod) {
            updateValueDependencies(dep.first, info);
        }
    }
    for (auto& dep : m_initialDependencies) {
        if (m_valueDependencies.find(dep.first) != m_valueDependencies.end()) {
            continue;
        }
        auto modRef = m_AAR.getModRefInfo(storeInst, dep.first, DL.getTypeStoreSize(dep.first->getType()));
        if (modRef == llvm::ModRefInfo::MRI_Mod) {
            updateValueDependencies(dep.first, info);
        }
    }
}

void BasicBlockAnalysisResult::updateRefAliasesDependencies(llvm::Instruction* instr, const ValueDepInfo& info)
{
    const auto& DL = instr->getModule()->getDataLayout();
    for (auto& dep : m_valueDependencies) {
        auto modRef = m_AAR.getModRefInfo(instr, dep.first, DL.getTypeStoreSize(dep.first->getType()));
        if (modRef == llvm::ModRefInfo::MRI_Ref) {
            updateValueDependencies(dep.first, info);
        }
        auto alias = m_AAR.alias(instr, dep.first);
        if (alias == llvm::AliasResult::NoAlias) {
            continue;
        } else {
            updateValueDependencies(dep.first, info);
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

bool BasicBlockAnalysisResult::isInputDependent(llvm::BasicBlock* block) const
{
    assert(block == m_BB);
    return m_is_inputDep;
}

bool BasicBlockAnalysisResult::isInputDependent(llvm::BasicBlock* block,
                                                const ArgumentDependenciesMap& depArgs) const
{
    return isInputDependent(block);
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
    return (deps.isInputArgumentDep() && Utils::haveIntersection(depArgs, deps.getArgumentDependencies()));
}

bool BasicBlockAnalysisResult::isInputIndependent(llvm::Instruction* instr) const
{
    assert(instr->getParent()->getParent() == m_F);
    return m_inputIndependentInstrs.find(instr) != m_inputIndependentInstrs.end();
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

const ValueDepInfo& BasicBlockAnalysisResult::getReturnValueDependencies() const
{
    return m_returnValueDependencies;
}

const BasicBlockAnalysisResult::ArgumentDependenciesMap&
BasicBlockAnalysisResult::getOutParamsDependencies() const
{
    return m_outArgDependencies;
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

void BasicBlockAnalysisResult::markAllInputDependent()
{
    m_is_inputDep = true;
    DepInfo info(DepInfo::INPUT_DEP);
    // out arg dependencies
    m_returnValueDependencies.updateValueDep(info);
    // function call arguments
    for (auto& functionItem : m_functionCallInfo) {
        functionItem.second.markAllInputDependent();
    }
    for (auto& depinstr : m_inputDependentInstrs) {
        depinstr.second = info;
    }
    for (auto& instr : m_inputIndependentInstrs) {
        m_inputDependentInstrs.insert(std::make_pair(instr, info));
    }
    m_inputIndependentInstrs.clear();
    for (auto& val : m_valueDependencies) {
        val.second.updateCompositeValueDep(info);
    }
}

long unsigned BasicBlockAnalysisResult::get_input_dep_count() const
{
    return m_finalInputDependentInstrs.size();
}

long unsigned BasicBlockAnalysisResult::get_input_indep_count() const
{
    return m_inputIndependentInstrs.size();
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
    DepInfo info;
    if (auto opinstr = llvm::dyn_cast<llvm::Instruction>(loadOp)) {
        if (!llvm::dyn_cast<llvm::AllocaInst>(opinstr)) {
            info = getInstructionDependencies(opinstr);
        }
    } else {
        info = getRefInfo(instr);
        if (!info.isDefined()) {
            info = getDependenciesFromAliases(loadOp);
        }
    }

    if (info.isDefined()) {
        return info;
    }
    llvm::Value* loadedValue = getMemoryValue(loadOp);
    if (loadedValue == nullptr) {
        if (llvm::dyn_cast<llvm::Constant>(loadOp)) {
            return DepInfo(DepInfo::INPUT_INDEP);
        }
        return getInstructionDependencies(llvm::dyn_cast<llvm::Instruction>(loadOp));
    }
    auto depInfo = getValueDependencies(loadedValue);
    if (!depInfo.isDefined()) {
        // might be unnecessary
        if (auto loadedValInstr = llvm::dyn_cast<llvm::Instruction>(loadedValue)) {
            return getInstructionDependencies(loadedValInstr);
        }
        auto globalVal = llvm::dyn_cast<llvm::GlobalVariable>(loadedValue);
        assert(globalVal != nullptr);
        m_referencedGlobals.insert(globalVal);
        return DepInfo(DepInfo::VALUE_DEP, ValueSet{globalVal});
    }
    assert(depInfo.isDefined());
    return depInfo.getValueDep();
}

DepInfo BasicBlockAnalysisResult::determineInstructionDependenciesFromOperands(llvm::Instruction* instr)
{
    DepInfo deps(DepInfo::INPUT_INDEP);
    for (auto op = instr->op_begin(); op != instr->op_end(); ++op) {
        if (auto* opInst = llvm::dyn_cast<llvm::Instruction>(op)) {
            const auto& value_dep = getValueDependencies(opInst);
            if (value_dep.isDefined()) {
                deps.mergeDependencies(value_dep.getValueDep());
            } else {
                const auto& c_deps = getInstructionDependencies(opInst);
                deps.mergeDependencies(c_deps);
            }
        } else if (auto* opVal = llvm::dyn_cast<llvm::Value>(op)) {
            if (auto* global = llvm::dyn_cast<llvm::GlobalVariable>(opVal)) {
                m_referencedGlobals.insert(global);
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

