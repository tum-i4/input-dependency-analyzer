#include "BasicBlockAnalysisResult.h"

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
    for (auto& I : *m_BB) {
        //llvm::dbgs() << "Instruction " << I << "\n";
        if (auto* allocInst = llvm::dyn_cast<llvm::AllocaInst>(&I)) {
            // Note alloc instructions are at the begining of the function
            // Here just collect them with unknown state
            m_valueDependencies[allocInst] = DepInfo(DepInfo::INPUT_INDEP);
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
        } else {
            processInstruction(&I);
        }
        processInstrForOutputArgs(&I);
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
        return DepInfo(DepInfo::DepInfo::INPUT_INDEP);
    }
    if (auto* loadInst = llvm::dyn_cast<llvm::LoadInst>(instr)) {
        return getLoadInstrDependencies(loadInst);
    }

    return determineInstructionDependenciesFromOperands(instr);
}

DepInfo BasicBlockAnalysisResult::getValueDependencies(llvm::Value* value)
{
    auto pos = m_valueDependencies.find(value);
    if (pos != m_valueDependencies.end()) {
        return pos->second;
    }
    auto initial_val_pos = m_initialDependencies.find(value);
    if (initial_val_pos != m_initialDependencies.end()) {
        m_valueDependencies[value] = initial_val_pos->second;
        return initial_val_pos->second;
    }
    return DepInfo();
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
    m_valueDependencies[value] = info;
    updateAliasesDependencies(value, info);
}

void BasicBlockAnalysisResult::updateReturnValueDependencies(const DepInfo& info)
{
    switch (info.getDependency()) {
    case DepInfo::INPUT_DEP:
    case DepInfo::INPUT_ARGDEP:
    case DepInfo::VALUE_DEP:
        m_returnValueDependencies.mergeDependencies(info);
        break;
    case DepInfo::INPUT_INDEP:
        break;
    default:
        assert(false);
    };
}

DepInfo BasicBlockAnalysisResult::getDependenciesFromAliases(llvm::Value* val)
{
    DepInfo info;
    for (const auto& dep : m_valueDependencies) {
        auto alias = m_AAR.alias(val, dep.first);
        if (alias != llvm::AliasResult::NoAlias) {
            info.mergeDependencies(dep.second);
        }
    }
    return info;
}

DepInfo BasicBlockAnalysisResult::getRefInfo(llvm::LoadInst* loadInst)
{
    DepInfo info;
    const auto& DL = loadInst->getModule()->getDataLayout();
    for (const auto& dep : m_valueDependencies) {
        auto modRef = m_AAR.getModRefInfo(loadInst, dep.first, DL.getTypeStoreSize(dep.first->getType()));
        if (modRef == llvm::ModRefInfo::MRI_Ref) {
            info.mergeDependencies(dep.second);
        }
    }
    return info;
}

void BasicBlockAnalysisResult::updateAliasesDependencies(llvm::Value* val, const DepInfo& info)
{
    for (auto& valDep : m_valueDependencies) {
        auto alias = m_AAR.alias(val, valDep.first);
        if (alias != llvm::AliasResult::NoAlias) {
            valDep.second = info;
        }
    }
}

void BasicBlockAnalysisResult::updateModAliasesDependencies(llvm::StoreInst* storeInst, const DepInfo& info)
{
    const auto& DL = storeInst->getModule()->getDataLayout();
    for (auto& dep : m_valueDependencies) {
        auto modRef = m_AAR.getModRefInfo(storeInst, dep.first, DL.getTypeStoreSize(dep.first->getType()));
        if (modRef == llvm::ModRefInfo::MRI_Mod) {
            updateValueDependencies(dep.first, info);
        }
    }
}

void BasicBlockAnalysisResult::setInitialValueDependencies(
                    const DependencyAnaliser::ValueDependencies& valueDependencies)
{
    m_initialDependencies = valueDependencies;
}

void BasicBlockAnalysisResult::setOutArguments(const DependencyAnaliser::ArgumentDependenciesMap& outArgs)
{
    m_outArgDependencies = outArgs;
}

bool BasicBlockAnalysisResult::isInputDependent(llvm::BasicBlock* block) const
{
    assert(block == m_BB);
    return m_is_inputDep;
}

bool BasicBlockAnalysisResult::isInputDependent(llvm::Instruction* instr) const
{
    assert(instr->getParent() == m_BB);
    if (m_finalized) {
        return m_finalInputDependentInstrs.find(instr) != m_finalInputDependentInstrs.end();
    }
    return m_inputDependentInstrs.find(instr) != m_inputDependentInstrs.end();
}

bool BasicBlockAnalysisResult::isInputIndependent(llvm::Instruction* instr) const
{
    assert(instr->getParent()->getParent() == m_F);
    return m_inputIndependentInstrs.find(instr) != m_inputIndependentInstrs.end();
}

bool BasicBlockAnalysisResult::hasValueDependencyInfo(llvm::Value* val) const
{
    auto pos = m_valueDependencies.find(val);
    if (pos != m_valueDependencies.end()) {
        return true;
    }
    return m_initialDependencies.find(val) != m_initialDependencies.end();
}

const DepInfo& BasicBlockAnalysisResult::getValueDependencyInfo(llvm::Value* val)
{
    auto pos = m_valueDependencies.find(val);
    if (pos != m_valueDependencies.end()) {
        return pos->second;
    }
    auto initial_val_pos = m_initialDependencies.find(val);
    // This is from external usage, through DependencyAnalysisResult interface
    assert(initial_val_pos != m_initialDependencies.end());
    // add referenced value
    m_valueDependencies[val] = initial_val_pos->second;
    return initial_val_pos->second;
}

DepInfo BasicBlockAnalysisResult::getInstructionDependencies(llvm::Instruction* instr) const
{
    auto pos = m_inputDependentInstrs.find(instr);
    if (pos == m_inputDependentInstrs.end()) {
        return DepInfo(DepInfo::INPUT_INDEP);
    }
    return pos->second;
}

const DependencyAnaliser::ValueDependencies& BasicBlockAnalysisResult::getValuesDependencies() const
{
    return m_valueDependencies;
}

const DepInfo& BasicBlockAnalysisResult::getReturnValueDependencies() const
{
    return m_returnValueDependencies;
}

const DependencyAnaliser::ArgumentDependenciesMap&
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
    m_returnValueDependencies = info;
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
        val.second = info;
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
    DepInfo info = getRefInfo(instr);
    if (info.isDefined()) {
        return info;
    }
    auto* loadOp = instr->getPointerOperand();
    if (auto opinstr = llvm::dyn_cast<llvm::Instruction>(loadOp)) {
        if (!llvm::dyn_cast<llvm::AllocaInst>(opinstr)) {
            info = getInstructionDependencies(opinstr);
        }
    } else {
        info = getDependenciesFromAliases(loadOp);
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
    return depInfo;
}

DepInfo BasicBlockAnalysisResult::determineInstructionDependenciesFromOperands(llvm::Instruction* instr)
{
    DepInfo deps(DepInfo::INPUT_INDEP);
    for (auto op = instr->op_begin(); op != instr->op_end(); ++op) {
        if (auto* opInst = llvm::dyn_cast<llvm::Instruction>(op)) {
            const auto& c_deps = getInstructionDependencies(opInst);
            deps.mergeDependencies(c_deps);
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
                deps.mergeDependencies(valDeps);
            }
        }
    }
    return deps;
}

} // namespace input_dependency

