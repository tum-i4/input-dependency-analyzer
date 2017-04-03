#include "FunctionAnaliser.h"

#include "BasicBlockAnalysisResult.h"
#include "DependencyAnalysisResult.h"
#include "DependencyAnaliser.h"
#include "LoopAnalysisResult.h"
#include "NonDeterministicBasicBlockAnaliser.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace input_dependency {

class FunctionAnaliser::Impl
{
public:
    Impl(llvm::Function* F,
         llvm::AAResults& AAR,
         llvm::LoopInfo& LI,
         const FunctionAnalysisGetter& getter)
        : m_F(F)
        , m_AAR(AAR)
        , m_LI(LI)
        , m_FAGetter(getter)
    {
    }

private:
    using ArgumentDependenciesMap = DependencyAnaliser::ArgumentDependenciesMap;
    using PredValDeps = DependencyAnalysisResult::InitialValueDpendencies;
    using PredArgDeps = DependencyAnalysisResult::InitialArgumentDependencies;
    using DependencyAnalysisResultT = std::unique_ptr<DependencyAnalysisResult>;
    using FunctionArgumentsDependencies = std::unordered_map<llvm::Function*, ArgumentDependenciesMap>;

public:
    bool isInputDependent(llvm::Instruction* instr) const;
    bool isOutArgInputDependent(llvm::Argument* arg) const;
    DepInfo getOutArgDependencies(llvm::Argument* arg) const;
    bool isReturnValueInputDependent() const;
    const DepInfo& getRetValueDependencies() const;

    void analize();
    void finalize(const ArgumentDependenciesMap& dependentArgNos);
    void dump() const;

    // Returns collected data for function calls in this function
    const DependencyAnaliser::ArgumentDependenciesMap& getCallArgumentInfo(llvm::Function* F) const
    {
        auto pos = m_calledFunctionsInfo.find(F);
        if (pos == m_calledFunctionsInfo.end()) {
            const_cast<Impl*>(this)->updateFunctionCallInfo(F);
        }
        pos = m_calledFunctionsInfo.find(F);
        return pos->second;
    }

    const FunctionArgumentsDependencies& getCallArgumentInfo() const
    {
        if (m_calledFunctionsInfo.empty()) {
            // not finalized yet? should not be called
            // assert(false)
            const_cast<Impl*>(this)->updateFunctionCallsInfo();
        }
        return m_calledFunctionsInfo;

    }

    FunctionSet getCallSitesData() const
    {
        return m_calledFunctions;
    }

    llvm::Function* getFunction()
    {
        return m_F;
    }

    const llvm::Function* getFunction() const
    {
        return m_F;
    }

private:
    void collectArguments();
    DependencyAnalysisResultT createBasicBlockAnalysisResult(llvm::BasicBlock* B);
    DepInfo getBasicBlockPredecessorInstructionsDeps(llvm::BasicBlock* B) const;
    void updateFunctionCallsInfo();
    void updateFunctionCallInfo(llvm::Function* F);
    void updateFunctionCallsInfo(llvm::BasicBlock* B);
    void updateFunctionCallInfo(llvm::BasicBlock* B, llvm::Function* F);
    void updateCalledFunctionsList(llvm::BasicBlock* B);
    void updateReturnValueDependencies(llvm::BasicBlock* B);
    void updateOutArgumentDependencies(llvm::BasicBlock* B);
    PredValDeps getBasicBlockPredecessorsDependencies(llvm::BasicBlock* B);
    PredArgDeps getBasicBlockPredecessorsArguments(llvm::BasicBlock* B);

private:
    llvm::Function* m_F;
    llvm::AAResults& m_AAR;
    llvm::LoopInfo& m_LI;
    const FunctionAnalysisGetter& m_FAGetter;
    Arguments m_inputs;
    DependencyAnaliser::ArgumentDependenciesMap m_outArgDependencies;
    DepInfo m_returnValueDependencies;
    FunctionArgumentsDependencies m_calledFunctionsInfo;
    FunctionSet m_calledFunctions;
    std::unordered_map<llvm::BasicBlock*, DependencyAnalysisResultT> m_BBAnalysisResults;
    // LoopInfo will be invalidated after analisis, instead of keeping copy of it, keep this map.
    std::unordered_map<llvm::BasicBlock*, llvm::BasicBlock*> m_loopBlocks;

    llvm::Loop* m_currentLoop;
}; // class FunctionAnaliser::Impl


bool FunctionAnaliser::Impl::isInputDependent(llvm::Instruction* instr) const
{
    auto bb = instr->getParent();
    assert(bb->getParent() == m_F);
    auto looppos = m_loopBlocks.find(bb);
    if (looppos != m_loopBlocks.end()) {
        bb = looppos->second;
    }
    auto pos = m_BBAnalysisResults.find(bb);
    assert(pos != m_BBAnalysisResults.end());
    return pos->second->isInputDependent(instr);
}

bool FunctionAnaliser::Impl::isOutArgInputDependent(llvm::Argument* arg) const
{
    auto pos = m_outArgDependencies.find(arg);
    if (pos == m_outArgDependencies.end()) {
        return false;
    }
    return pos->second.isInputDep() || pos->second.isInputArgumentDep();
}

DepInfo FunctionAnaliser::Impl::getOutArgDependencies(llvm::Argument* arg) const
{
    auto pos = m_outArgDependencies.find(arg);
    if (pos == m_outArgDependencies.end()) {
        return DepInfo();
    }
    return pos->second;
}

bool FunctionAnaliser::Impl::isReturnValueInputDependent() const
{
    return m_returnValueDependencies.isInputDep() || m_returnValueDependencies.isInputArgumentDep();
}

const DepInfo& FunctionAnaliser::Impl::getRetValueDependencies() const
{
    return m_returnValueDependencies;
}

void FunctionAnaliser::Impl::analize()
{
    collectArguments();
    auto it = m_F->begin();
    for (; it != m_F->end(); ++it) {
        auto bb = &*it;
        if (m_LI.isLoopHeader(bb)) {
            auto loop = m_LI.getLoopFor(bb);
            if (loop->getParentLoop() != nullptr) {
                m_loopBlocks[bb] = m_currentLoop->getHeader();
                continue;
            }
            m_currentLoop = loop;
            // One option is having one loop analiser, mapped to the header of the loop.
            // Another opetion is mapping all blocks of the loop to the same analiser.
            // this is implementatin of the first option.
            m_BBAnalysisResults[bb].reset(new LoopAnalysisResult(m_F, m_AAR, m_inputs, m_FAGetter, *m_currentLoop, m_LI));
        } else if (auto loop = m_LI.getLoopFor(bb)) {
            m_loopBlocks[bb] = m_currentLoop->getHeader();
            continue;
        } else {
            //m_currentLoop = nullptr;
            m_BBAnalysisResults[bb] = createBasicBlockAnalysisResult(bb);
        }
        m_BBAnalysisResults[bb]->setInitialValueDependencies(getBasicBlockPredecessorsDependencies(bb));
        m_BBAnalysisResults[bb]->setOutArguments(getBasicBlockPredecessorsArguments(bb));
        m_BBAnalysisResults[bb]->gatherResults();

        updateCalledFunctionsList(bb);
        updateReturnValueDependencies(bb);
        updateOutArgumentDependencies(bb);
    }
    m_inputs.clear();
}

void FunctionAnaliser::Impl::finalize(const ArgumentDependenciesMap& dependentArgs)
{
    for (auto& item : m_BBAnalysisResults) {
        item.second->finalizeResults(dependentArgs);
        updateFunctionCallsInfo(item.first);
    }
}

void FunctionAnaliser::Impl::dump() const
{
    llvm::dbgs() << "****** Function " << m_F->getName() << " ******\\n";
    for (auto& BB : *m_F) {
        auto pos = m_BBAnalysisResults.find(&BB);
        if (pos != m_BBAnalysisResults.end()) {
            pos->second->dumpResults();
        }
    }
}

void FunctionAnaliser::Impl::collectArguments()
{
    auto& arguments = m_F->getArgumentList();
    std::for_each(arguments.begin(), arguments.end(),
            [this] (llvm::Argument& arg) {
                this->m_inputs.push_back(&arg);
                llvm::Value* val = llvm::dyn_cast<llvm::Value>(&arg);
                if (val->getType()->isPointerTy()) {
                    m_outArgDependencies[&arg] = DepInfo(DepInfo::INPUT_ARGDEP, ArgumentSet{&arg});
                }
            });
}

FunctionAnaliser::Impl::DependencyAnalysisResultT
FunctionAnaliser::Impl::createBasicBlockAnalysisResult(llvm::BasicBlock* B)
{
    const auto& depInfo = getBasicBlockPredecessorInstructionsDeps(B);
    if (depInfo.isInputDep() || depInfo.isInputArgumentDep()) {
        return DependencyAnalysisResultT(
                new NonDeterministicBasicBlockAnaliser(m_F, m_AAR, m_inputs, m_FAGetter, B, depInfo));
    }
    return DependencyAnalysisResultT(new BasicBlockAnalysisResult(m_F, m_AAR, m_inputs, m_FAGetter, B));
}

DepInfo FunctionAnaliser::Impl::getBasicBlockPredecessorInstructionsDeps(llvm::BasicBlock* B) const
{
    DepInfo dep(DepInfo::DepInfo::INPUT_INDEP);
    auto pred = pred_begin(B);
    while (pred != pred_end(B)) {
        auto pb = *pred;
        const auto& termInstr = pb->getTerminator();
        if (termInstr == nullptr) {
            dep.setDependency(DepInfo::DepInfo::INPUT_ARGDEP);
            break;
        }
        // if all terminating instructions leading to this block are unconditional, this block will be executed not depending on input.
        if (auto* branchInstr = llvm::dyn_cast<llvm::BranchInst>(termInstr)) {
            if (branchInstr->isUnconditional()) {
                ++pred;
                continue;
            }
        }

        auto pos = m_BBAnalysisResults.find(pb);
        if (pos == m_BBAnalysisResults.end()) {
            assert(m_LI.getLoopFor(pb) != nullptr);
            auto loopHead = m_loopBlocks.find(pb);
            if (loopHead == m_loopBlocks.end()) {
                ++pred;
                continue;
            }
            pos = m_BBAnalysisResults.find(loopHead->second);
            //assert(m_currentLoop != nullptr);
            //assert(*pred == m_currentLoop->getLoopLatch());
        }
        assert(pos != m_BBAnalysisResults.end());
        if (pos->second->isInputDependent(termInstr)) {
            dep.setDependency(DepInfo::INPUT_ARGDEP);
            dep.mergeDependencies(pos->second->getInstructionDependencies(termInstr));
        }
        ++pred;
    }
    return dep;
}

void FunctionAnaliser::Impl::updateFunctionCallsInfo()
{
    for (const auto& B : m_BBAnalysisResults) {
        updateFunctionCallsInfo(B.first);
    }
}

void FunctionAnaliser::Impl::updateFunctionCallInfo(llvm::Function* F)
{
    for (const auto& B : m_BBAnalysisResults) {
        updateFunctionCallInfo(B.first, F);
    }
}

void FunctionAnaliser::Impl::updateFunctionCallsInfo(llvm::BasicBlock* B)
{
    const auto& info = m_BBAnalysisResults[B]->getFunctionsCallInfo();
    for (const auto& item : info) {
        auto argDeps = item.second.getMergedDependencies();
        auto res = m_calledFunctionsInfo.insert(std::make_pair(item.first, argDeps));
        if (!res.second) {
            for (auto& deps : res.first->second) {
                deps.second.mergeDependencies(argDeps[deps.first]);
            }
        }
    }
}

void FunctionAnaliser::Impl::updateFunctionCallInfo(llvm::BasicBlock* B, llvm::Function* F)
{
    const auto& BA = m_BBAnalysisResults[B];
    if (!BA->hasFunctionCallInfo(F)) {
        return;
    }
    const auto& info = BA->getFunctionCallInfo(F);
    auto argDeps = info.getMergedDependencies();
    auto res = m_calledFunctionsInfo.insert(std::make_pair(F, argDeps));
    if (!res.second) {
        for (auto& deps : argDeps) {
            auto r = res.first->second.insert(deps);
            if (!r.second) {
                r.first->second.mergeDependencies(argDeps[r.first->first]);
            }
        }
//        if (res.first->second.empty()) {
//            res.first->second.insert(argDeps);
//            return;
//        }
//        for (auto& deps : res.first->second) {
//            deps.second.mergeDependencies(argDeps[deps.first]);
//        }
    }
}

void FunctionAnaliser::Impl::updateCalledFunctionsList(llvm::BasicBlock* B)
{
    const auto& calledFunctions = m_BBAnalysisResults[B]->getCallSitesData();
    m_calledFunctions.insert(calledFunctions.begin(), calledFunctions.end());
}

void FunctionAnaliser::Impl::updateReturnValueDependencies(llvm::BasicBlock* B)
{
    const auto& retValDeps = m_BBAnalysisResults[B]->getReturnValueDependencies();
    if (retValDeps.getDependency() > m_returnValueDependencies.getDependency()) {
        m_returnValueDependencies.setDependency(retValDeps.getDependency());
    }
    m_returnValueDependencies.mergeDependencies(retValDeps);
}

void FunctionAnaliser::Impl::updateOutArgumentDependencies(llvm::BasicBlock* B)
{
    const auto& outArgDeps = m_BBAnalysisResults[B]->getOutParamsDependencies();
    for (const auto& item : outArgDeps) {
        auto pos = m_outArgDependencies.find(item.first);
        assert(pos != m_outArgDependencies.end());
        pos->second = item.second;
    }
}

FunctionAnaliser::Impl::PredValDeps
FunctionAnaliser::Impl::getBasicBlockPredecessorsDependencies(llvm::BasicBlock* B)
{
    PredValDeps deps;
    auto pred = pred_begin(B);
    while (pred != pred_end(B)) {
        auto pos = m_BBAnalysisResults.find(*pred);
        if (pos == m_BBAnalysisResults.end()) {
            assert(m_LI.getLoopFor(*pred) != nullptr);
            auto loopHead = m_loopBlocks.find(*pred);
            if (loopHead == m_loopBlocks.end()) {
                ++pred;
                continue;
            }
            pos = m_BBAnalysisResults.find(loopHead->second);
        }
        assert(pos != m_BBAnalysisResults.end());
        const auto& valueDeps = pos->second->getValuesDependencies();
        for (auto& dep : valueDeps) {
            deps[dep.first].push_back(dep.second);
        }
        ++pred;
    }
    return deps;
}

FunctionAnaliser::Impl::PredArgDeps
FunctionAnaliser::Impl::getBasicBlockPredecessorsArguments(llvm::BasicBlock* B)
{
    PredArgDeps deps;
    auto pred = pred_begin(B);
    // entry block
    if (pred == pred_end(B)) {
        for (auto& item : m_outArgDependencies) {
            deps[item.first].push_back(item.second);
        }
        return deps;
    }
    while (pred != pred_end(B)) {
        auto pos = m_BBAnalysisResults.find(*pred);
        if (pos == m_BBAnalysisResults.end()) {
            assert(m_LI.getLoopFor(*pred) != nullptr);
            auto loopHead = m_loopBlocks.find(*pred);
            if (loopHead == m_loopBlocks.end()) {
                ++pred;
                continue;
            }
            pos = m_BBAnalysisResults.find(loopHead->second);
        }
        assert(pos != m_BBAnalysisResults.end());
        const auto& argDeps = pos->second->getOutParamsDependencies();
        for (const auto& dep : argDeps) {
            deps[dep.first].push_back(dep.second);
        }
        ++pred;
    }
    return deps;
}

FunctionAnaliser::FunctionAnaliser(llvm::Function* F,
                                   llvm::AAResults& AAR,
                                   llvm::LoopInfo& LI,
                                   const FunctionAnalysisGetter& getter)
    : m_analiser(new Impl(F, AAR, LI, getter))
{
}

void FunctionAnaliser::analize()
{
    m_analiser->analize();
}

void FunctionAnaliser::finalize(const DependencyAnaliser::ArgumentDependenciesMap& dependentArgNos)
{
    m_analiser->finalize(dependentArgNos);
}

FunctionSet FunctionAnaliser::getCallSitesData() const
{
    return m_analiser->getCallSitesData();
}

DependencyAnaliser::ArgumentDependenciesMap
FunctionAnaliser::getCallArgumentInfo(llvm::Function* F) const
{
    return m_analiser->getCallArgumentInfo(F);
}

bool FunctionAnaliser::isInputDependent(llvm::Instruction* instr) const
{
    return m_analiser->isInputDependent(instr);
}

bool FunctionAnaliser::isInputDependent(const llvm::Instruction* instr) const
{
    return m_analiser->isInputDependent(const_cast<llvm::Instruction*>(instr));
}

bool FunctionAnaliser::isOutArgInputDependent(llvm::Argument* arg) const
{
    return m_analiser->isOutArgInputDependent(arg);
}

DepInfo FunctionAnaliser::getOutArgDependencies(llvm::Argument* arg) const
{
    return m_analiser->getOutArgDependencies(arg);
}

bool FunctionAnaliser::isReturnValueInputDependent() const
{
    return m_analiser->isReturnValueInputDependent();
}

const DepInfo& FunctionAnaliser::getRetValueDependencies() const
{
    return m_analiser->getRetValueDependencies();
}

void FunctionAnaliser::dump() const
{
    m_analiser->dump();
}

llvm::Function* FunctionAnaliser::getFunction()
{
    return m_analiser->getFunction();
}

const llvm::Function* FunctionAnaliser::getFunction() const
{
    return m_analiser->getFunction();
}

} // namespace input_dependency

