#include "BasicBlockAnalysisResult.h"

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

BasicBlockAnalysisResult::BasicBlockAnalysisResult(llvm::Function* F,
                                                   llvm::AAResults& AAR,
                                                   const Arguments& inputs,
                                                   const FunctionAnalysisGetter& Fgetter,
                                                   llvm::BasicBlock* BB)
                                : DependencyAnaliser(F, AAR, inputs, Fgetter)
                                , m_BB(BB)
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
            m_valueDependencies[allocInst];
        } else if (auto* retInst = llvm::dyn_cast<llvm::ReturnInst>(&I)) {
            processReturnInstr(retInst);
        }  else if (auto* branchInst = llvm::dyn_cast<llvm::BranchInst>(&I)) {
            processBranchInst(branchInst);
        } else if (auto* storeInst = llvm::dyn_cast<llvm::StoreInst>(&I)) {
            processStoreInst(storeInst);
        } else if (auto* callInst = llvm::dyn_cast<llvm::CallInst>(&I)) {
            processCallInst(callInst);
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
        return DepInfo(DepInfo::DepInfo::INPUT_DEP, deppos->second);
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
    return m_valueDependencies[value];
}

void BasicBlockAnalysisResult::updateInstructionDependencies(llvm::Instruction* instr, const DepInfo& info)
{
    switch (info.getDependency()) {
    case DepInfo::DepInfo::INPUT_DEP:
        m_inputDependentInstrs[instr].insert(info.getArgumentDependencies().begin(),
                                             info.getArgumentDependencies().end());
        break;
    case DepInfo::DepInfo::INPUT_INDEP:
        m_inputIndependentInstrs.insert(instr);
        break;
    default:
        assert(false);
    };
}

void BasicBlockAnalysisResult::updateValueDependencies(llvm::Value* value, const DepInfo& info)
{
    assert(info.isInputDep() || info.isInputIndep());
    m_valueDependencies[value] = info;
}

void BasicBlockAnalysisResult::updateReturnValueDependencies(const DepInfo& info)
{
    switch (info.getDependency()) {
    case DepInfo::INPUT_DEP:
        m_returnValueDependencies.setDependency(info.getDependency());
        m_returnValueDependencies.mergeDependencies(info.getArgumentDependencies());
        break;
    case DepInfo::INPUT_INDEP:
        break;
    default:
        assert(false);
    };
}

void BasicBlockAnalysisResult::setInitialValueDependencies(
                    const DependencyAnalysisResult::InitialValueDpendencies& valueDependencies)
{
    // In practice number of predecessors will be at most 2
    for (const auto& item : valueDependencies) {
        auto& valDep = m_valueDependencies[item.first];
        for (const auto& dep : item.second) {
            if (valDep.getDependency() <= dep.getDependency()) {
                valDep.setDependency(dep.getDependency());
                valDep.mergeDependencies(dep);
            }
        }
    }
}

void BasicBlockAnalysisResult::setOutArguments(const DependencyAnalysisResult::InitialArgumentDependencies& outArgs)
{
    for (const auto& arg : outArgs) {
        auto& argDep = m_outArgDependencies[arg.first];
        for (const auto& dep : arg.second) {
            if (argDep.getDependency() <= dep.getDependency()) {
                argDep.setDependency(dep.getDependency());
                argDep.mergeDependencies(dep);
            }
        }
    }
}

bool BasicBlockAnalysisResult::isInputDependent(llvm::Instruction* instr) const
{
    assert(instr->getParent()->getParent() == m_F);
    if (m_finalized) {
        return m_finalInputDependentInstrs.find(instr) != m_finalInputDependentInstrs.end();
    }
    return m_inputDependentInstrs.find(instr) != m_inputDependentInstrs.end();
 }

const ArgumentSet& BasicBlockAnalysisResult::getValueInputDependencies(llvm::Value* val) const
{
    auto pos = m_valueDependencies.find(val);
    assert(pos != m_valueDependencies.end());
    return pos->second.getArgumentDependencies();
}

DepInfo BasicBlockAnalysisResult::getInstructionDependencies(llvm::Instruction* instr) const
{
    auto pos = m_inputDependentInstrs.find(instr);
    if (pos == m_inputDependentInstrs.end()) {
        return DepInfo(DepInfo::INPUT_INDEP);
    }
    return DepInfo(DepInfo::INPUT_DEP, pos->second);
}

const DependencyAnaliser::ValueDependencies& BasicBlockAnalysisResult::getValuesDependencies() const
{
    return m_valueDependencies;
}

//const ValueSet& BasicBlockAnalysisResult::getValueDependencies(llvm::Value* val)
//{
//    return ValueSet();
//}

const DepInfo& BasicBlockAnalysisResult::getReturnValueDependencies() const
{
    return m_returnValueDependencies;
}

const DependencyAnaliser::ArgumentDependenciesMap&
BasicBlockAnalysisResult::getOutParamsDependencies() const
{
    return m_outArgDependencies;
}

const DependencyAnaliser::FunctionArgumentsDependencies&
BasicBlockAnalysisResult::getFunctionsCallInfo() const
{
    return m_calledFunctionsInfo;
}

DepInfo BasicBlockAnalysisResult::getLoadInstrDependencies(llvm::LoadInst* instr)
{
    auto* loadOp = instr->getPointerOperand();
    llvm::Value* loadedValue = getMemoryValue(loadOp);
    if (loadedValue == nullptr) {
        return getInstructionDependencies(llvm::dyn_cast<llvm::Instruction>(loadOp));
    }
    auto pos = m_valueDependencies.find(loadedValue);
    assert(pos != m_valueDependencies.end());
    return pos->second;
}

DepInfo BasicBlockAnalysisResult::determineInstructionDependenciesFromOperands(llvm::Instruction* instr)
{
    ArgumentSet deps;
    DepInfo::Dependency state = DepInfo::INPUT_INDEP;
    for (auto op = instr->op_begin(); op != instr->op_end(); ++op) {
        if (auto* opInst = llvm::dyn_cast<llvm::Instruction>(op)) {
            const auto& c_deps = getInstructionDependencies(opInst);
            if (c_deps.isInputDep()) {
                state = DepInfo::INPUT_DEP;
                const auto& argDeps = c_deps.getArgumentDependencies();
                deps.insert(argDeps.begin(), argDeps.end());
            }
        } else if (auto* opVal = llvm::dyn_cast<llvm::Value>(op)) {
            if (auto* c_arg = isInput(opVal)) {
                state = DepInfo::INPUT_DEP;
                deps.insert(c_arg);
            }
        }
    }
    if (state == DepInfo::INPUT_INDEP) {
        assert(deps.empty());
    }
    return DepInfo(state, deps);
}

} // namespace input_dependency

