#include "LoopAnalysisResult.h"

#include "ReflectingBasicBlockAnaliser.h"
#include "NonDeterministicReflectingBasicBlockAnaliser.h"
#include "Utils.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
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

LoopAnalysisResult::LoopAnalysisResult(llvm::Function* F,
                                       llvm::AAResults& AAR,
                                       const Arguments& inputs,
                                       const FunctionAnalysisGetter& Fgetter,
                                       llvm::Loop& L,
                                       llvm::LoopInfo& LI)
                                : m_F(F)
                                , m_AAR(AAR)
                                , m_inputs(inputs)
                                , m_FAG(Fgetter)
                                , m_L(L)
                                , m_LI(LI)
{
}

void LoopAnalysisResult::gatherResults()
{
    const auto& blocks = m_L.getBlocks();
    for (const auto& B : blocks) {
        m_BBAnalisers[B] = createReflectingBasicBlockAnaliser(B);
        m_BBAnalisers[B]->setInitialValueDependencies(getBasicBlockPredecessorsDependencies(B));
        m_BBAnalisers[B]->setOutArguments(getBasicBlockPredecessorsArguments(B));
        m_BBAnalisers[B]->gatherResults();
        //m_BBAnalisers[B]->dumpResults();
    }
    reflect();
    updateCalledFunctionsList();
    updateReturnValueDependencies();
    updateOutArgumentDependencies();
    updateValueDependencies();
}

void LoopAnalysisResult::finalizeResults(const DependencyAnaliser::ArgumentDependenciesMap& dependentArgs)
{
    for (auto& item : m_BBAnalisers) {
        item.second->finalizeResults(dependentArgs);
    }
    updateFunctionCallInfo();
}

void LoopAnalysisResult::dumpResults() const
{
    for (const auto& item : m_BBAnalisers) {
        item.second->dumpResults();
    }
}

void LoopAnalysisResult::setInitialValueDependencies(
            const DependencyAnalysisResult::InitialValueDpendencies& valueDependencies)
{
    // In practice number of predecessors will be at most 2
    for (const auto& item : valueDependencies) {
        auto& valDep = m_valueDependencies[item.first];
        for (const auto& dep : item.second) {
            if (valDep.getDependency() <= dep.getDependency()) {
                valDep.setDependency(dep.getDependency());
                valDep.getArgumentDependencies().insert(dep.getArgumentDependencies().begin(),
                                                        dep.getArgumentDependencies().end());
            }
        }
    }
}

void LoopAnalysisResult::setOutArguments(const DependencyAnalysisResult::InitialArgumentDependencies& outArgs)
{
    for (const auto& arg : outArgs) {
        auto& argDep = m_outArgDependencies[arg.first];
        for (const auto& dep : arg.second) {
            if (argDep.getDependency() <= dep.getDependency()) {
                argDep.setDependency(dep.getDependency());
                argDep.mergeDependencies(dep.getArgumentDependencies());
            }
        }
    }
}

bool LoopAnalysisResult::isInputDependent(llvm::Instruction* instr) const
{
    auto parentBB = instr->getParent();
    auto pos = m_BBAnalisers.find(parentBB);
    assert(pos != m_BBAnalisers.end());
    return pos->second->isInputDependent(instr);
}

const ArgumentSet& LoopAnalysisResult::getValueInputDependencies(llvm::Value* val) const
{
    auto pos = m_valueDependencies.find(val);
    assert(pos != m_valueDependencies.end());
    return pos->second.getArgumentDependencies();
}

DepInfo LoopAnalysisResult::getInstructionDependencies(llvm::Instruction* instr) const
{
    auto parentBB = instr->getParent();
    auto pos = m_BBAnalisers.find(parentBB);
    assert(pos != m_BBAnalisers.end());
    return pos->second->getInstructionDependencies(instr);
}

const DependencyAnaliser::ValueDependencies& LoopAnalysisResult::getValuesDependencies() const
{
    return m_valueDependencies;
}

const DepInfo& LoopAnalysisResult::getReturnValueDependencies() const
{
    return m_returnValueDependencies;
}

const DependencyAnaliser::ArgumentDependenciesMap&
LoopAnalysisResult::getOutParamsDependencies() const
{
    return m_outArgDependencies;
}

const LoopAnalysisResult::FCallsArgDeps& LoopAnalysisResult::getFunctionsCallInfo() const
{
    if (m_functionCallInfo.empty()) {
        const_cast<LoopAnalysisResult*>(this)->updateFunctionCallInfo();
    }
    return m_functionCallInfo;
}

const FunctionCallDepInfo& LoopAnalysisResult::getFunctionCallInfo(llvm::Function* F) const
{
    auto pos = m_functionCallInfo.find(F);
    if (pos == m_functionCallInfo.end()) {
        const_cast<LoopAnalysisResult*>(this)->updateFunctionCallInfo(F);
    }
    pos = m_functionCallInfo.find(F);
    return pos->second;
}

bool LoopAnalysisResult::hasFunctionCallInfo(llvm::Function* F) const
{
    auto pos = m_functionCallInfo.find(F);
    if (pos == m_functionCallInfo.end()) {
        const_cast<LoopAnalysisResult*>(this)->updateFunctionCallInfo(F);
    }
    return m_functionCallInfo.find(F) != m_functionCallInfo.end();
}

const FunctionSet& LoopAnalysisResult::getCallSitesData() const
{
    return m_calledFunctions;
}

LoopAnalysisResult::PredValDeps LoopAnalysisResult::getBasicBlockPredecessorsDependencies(llvm::BasicBlock* B)
{
    PredValDeps deps;

    if (m_L.getHeader() == B) {
        for (const auto& item : m_valueDependencies) {
            deps[item.first].push_back(item.second);
        }
        return deps;
    }
    auto pred = pred_begin(B);
    while (pred != pred_end(B)) {
        auto pos = m_BBAnalisers.find(*pred);
        if (pos == m_BBAnalisers.end()) {
            ++pred;
            continue;
        }
        assert(pos != m_BBAnalisers.end());
        const auto& valueDeps = pos->second->getValuesDependencies();
        for (auto& dep : valueDeps) {
            deps[dep.first].push_back(dep.second);
        }
        ++pred;
    }
    return deps;
}

LoopAnalysisResult::PredArgDeps LoopAnalysisResult::getBasicBlockPredecessorsArguments(llvm::BasicBlock* B)
{
    PredArgDeps deps;
    auto pred = pred_begin(B);
    while (pred != pred_end(B)) {
        if (!m_L.contains(*pred)) {
            for (const auto& item : m_outArgDependencies) {
                deps[item.first].push_back(item.second);
            }
            ++pred;
            continue;
        }
        auto pos = m_BBAnalisers.find(*pred);
        if (pos == m_BBAnalisers.end()) {
            ++pred;
            continue;
        }
        assert(pos != m_BBAnalisers.end());
        const auto& argDeps = pos->second->getOutParamsDependencies();
        for (const auto& dep : argDeps) {
            deps[dep.first].push_back(dep.second);
        }
        ++pred;
    }
    return deps;
}

void LoopAnalysisResult::updateFunctionCallInfo()
{
    for (const auto& item : m_BBAnalisers) {
        auto callInfo = item.second->getFunctionsCallInfo();
        for (auto callItem : callInfo) {
            auto res = m_functionCallInfo.insert(callItem);
            if (res.second) {
                continue;
            }
            res.first->second.addCalls(callItem.second);

        }
    }
}

void LoopAnalysisResult::updateFunctionCallInfo(llvm::Function* F)
{
    for (const auto& item : m_BBAnalisers) {
        if (!item.second->hasFunctionCallInfo(F)) {
            continue;
        }
        auto callInfo = item.second->getFunctionCallInfo(F);
        auto res = m_functionCallInfo.insert(std::make_pair(F, callInfo));
        if (!res.second) {
            res.first->second.addCalls(callInfo);
        }
    }
}

void LoopAnalysisResult::updateCalledFunctionsList()
{
    for (const auto& BA : m_BBAnalisers) {
        const auto& calledFunctions = BA.second->getCallSitesData();
        m_calledFunctions.insert(calledFunctions.begin(), calledFunctions.end());
    }
}

void LoopAnalysisResult::updateReturnValueDependencies()
{
    for (const auto& BA : m_BBAnalisers) {
        const auto& retValDeps = BA.second->getReturnValueDependencies();
        m_returnValueDependencies.mergeDependencies(retValDeps);
    }
}

void LoopAnalysisResult::updateOutArgumentDependencies()
{
    // Out args are the same for all blocks,
    // after reflection all blocks will contains same information for out args.
    // So just pick one of blocks (say header) and get dep info from it
    auto BB = m_L.getHeader();
    const auto& outArgs = m_BBAnalisers[BB]->getOutParamsDependencies();
    for (const auto& item : outArgs) {
        auto pos = m_outArgDependencies.find(item.first);
        assert(pos != m_outArgDependencies.end());
        pos->second = item.second;
    }
}

void LoopAnalysisResult::updateValueDependencies()
{
    auto BB = m_L.getHeader();
    const auto& valuesDeps = m_BBAnalisers[BB]->getValuesDependencies();
    for (auto& item : m_valueDependencies) {
        auto pos = valuesDeps.find(item.first);
        if (pos != valuesDeps.end()) {
            item.second.mergeDependencies(pos->second);
        }
    }
}

void LoopAnalysisResult::reflect()
{
    BlocksVector latches;
    m_L.getLoopLatches(latches);
    for (const auto& subloop : m_L) {
        subloop->getLoopLatches(latches);
    }
    DependencyAnaliser::ValueDependencies valueDependencies;
    for (const auto& latch : latches) {
        auto valueDeps = m_BBAnalisers[latch]->getValuesDependencies();
        for (const auto& dep : valueDeps) {
            auto res = valueDependencies.insert(dep);
            if (!res.second) {
                res.first->second.mergeDependencies(dep.second);
            }
        }
    }

    const auto& blocks = m_L.getBlocks();
    auto it = blocks.begin();
    while (it != blocks.end()) {
        auto B = *it;
        m_BBAnalisers[B]->reflect(valueDependencies);
        ++it;
    }
}

LoopAnalysisResult::ReflectingDependencyAnaliserT LoopAnalysisResult::createReflectingBasicBlockAnaliser(llvm::BasicBlock* B)
{
    auto depInfo = getBasicBlockDeps(B);
    addLoopDependencies(B, depInfo);
    if (!depInfo.isInputIndep()) {
        return ReflectingDependencyAnaliserT(new NonDeterministicReflectingBasicBlockAnaliser(m_F, m_AAR, m_inputs, m_FAG, B, depInfo));
    }
    return ReflectingDependencyAnaliserT(new ReflectingBasicBlockAnaliser(m_F, m_AAR, m_inputs, m_FAG, B));
}

DepInfo LoopAnalysisResult::getBasicBlockDeps(llvm::BasicBlock* B) const
{
    DepInfo dep(DepInfo::INPUT_INDEP);
    auto pred = pred_begin(B);
    while (pred != pred_end(B)) {
        auto pb = *pred;
        const auto& deps = getBlockTerminatingDependencies(pb);
        dep.mergeDependencies(deps);
        ++pred;
    }
    return dep;
}

void LoopAnalysisResult::addLoopDependencies(llvm::BasicBlock* B, DepInfo& depInfo) const
{
    auto loop = m_LI.getLoopFor(B);
    while (loop != nullptr) {
        auto headerBlock = loop->getHeader();
        addLoopDependency(headerBlock, depInfo);
        loop = loop->getParentLoop();
    }
    loop = m_LI.getLoopFor(B);
    BlocksVector latches;
    loop->getLoopLatches(latches);
    for (const auto& latch : latches) {
        addLoopDependency(latch, depInfo);
    }

    BlocksVector exitBlocks;
    loop->getExitingBlocks(exitBlocks);
    for (const auto& exitingB : exitBlocks) {
        addLoopDependency(exitingB, depInfo);
    }
}

void LoopAnalysisResult::addLoopDependency(llvm::BasicBlock* B, DepInfo& depInfo) const
{
    auto deps = getBlockTerminatingDependencies(B);
    if (!deps.isDefined()) {
        deps = getBasicBlockDeps(B);
    }
    depInfo.mergeDependencies(deps);
}

DepInfo LoopAnalysisResult::getBlockTerminatingDependencies(llvm::BasicBlock* B) const
{
    const auto& termInstr = B->getTerminator();
    if (termInstr == nullptr) {
        return DepInfo(DepInfo::INPUT_DEP);
    }
    if (auto* branchInstr = llvm::dyn_cast<llvm::BranchInst>(termInstr)) {
        if (branchInstr->isUnconditional()) {
            return DepInfo();
        }
    }
    auto pos = m_BBAnalisers.find(B);
    if (pos == m_BBAnalisers.end()) {
        ValueSet values = Utils::dissolveInstruction(termInstr);
        return DepInfo(DepInfo::VALUE_DEP, values);
    }
    return pos->second->getInstructionDependencies(termInstr);
}

} // namespace input_dependency

