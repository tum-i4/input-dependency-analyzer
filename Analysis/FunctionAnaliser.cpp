#include "FunctionAnaliser.h"

#include "BasicBlockAnalysisResult.h"
#include "DependencyAnalysisResult.h"
#include "DependencyAnaliser.h"
#include "LoopAnalysisResult.h"
#include "InputDependentBasicBlockAnaliser.h"
#include "NonDeterministicBasicBlockAnaliser.h"
#include "IndirectCallSitesAnalysis.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/PostDominators.h"
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
         const llvm::PostDominatorTree& PDom,
         const VirtualCallSiteAnalysisResult& virtualCallsInfo,
         const IndirectCallSitesAnalysisResult& indirectCallsInfo,
         const FunctionAnalysisGetter& getter)
        : m_F(F)
        , m_AAR(AAR)
        , m_LI(LI)
        , m_postDomTree(PDom)
        , m_virtualCallsInfo(virtualCallsInfo)
        , m_indirectCallsInfo(indirectCallsInfo)
        , m_FAGetter(getter)
        , m_globalsUpdated(false)
    {
    }

    void setFunction(llvm::Function* F)
    {
        m_F = F;
    }

private:
    using ArgumentDependenciesMap = DependencyAnaliser::ArgumentDependenciesMap;
    using GlobalVariableDependencyMap = DependencyAnaliser::GlobalVariableDependencyMap;
    using DependencyAnalysisResultT = std::unique_ptr<DependencyAnalysisResult>;
    using FunctionArgumentsDependencies = std::unordered_map<llvm::Function*, ArgumentDependenciesMap>;
    using FunctionGlobalsDependencies = std::unordered_map<llvm::Function*, GlobalVariableDependencyMap>;

public:
    bool isInputDependent(llvm::Instruction* instr) const;
    bool isInputIndependent(llvm::Instruction* instr) const;
    bool isOutArgInputIndependent(llvm::Argument* arg) const;
    DepInfo getOutArgDependencies(llvm::Argument* arg) const;
    bool isReturnValueInputIndependent() const;
    const DepInfo& getRetValueDependencies() const;
    bool hasGlobalVariableDepInfo(llvm::GlobalVariable* global) const;
    const DepInfo& getGlobalVariableDependencies(llvm::GlobalVariable* global) const;
    DepInfo getDependencyInfoFromBlock(llvm::Value* val, llvm::BasicBlock* block) const;
    // Returns collected data for function calls in this function
    const DependencyAnaliser::ArgumentDependenciesMap& getCallArgumentInfo(llvm::Function* F) const;
    FunctionCallDepInfo getFunctionCallDepInfo(llvm::Function* F) const;
    const DependencyAnaliser::GlobalVariableDependencyMap& getCallGlobalsInfo(llvm::Function* F) const;
    const GlobalsSet& getReferencedGlobals() const;
    const GlobalsSet& getModifiedGlobals() const;

    void analize();
    void finalizeArguments(const ArgumentDependenciesMap& dependentArgNos);
    void finalizeGlobals(const GlobalVariableDependencyMap& globalsDeps);
    void dump() const;

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

    void updateFunctionCallInfo(llvm::Function* F);
    void updateFunctionCallsInfo(llvm::BasicBlock* B);
    void updateFunctionCallInfo(llvm::BasicBlock* B, llvm::Function* F);
    void updateFunctionCallGlobalsInfo(llvm::Function* F);
    void updateFunctionCallsGlobalsInfo(llvm::BasicBlock* B);
    void updateFunctionCallGlobalsInfo(llvm::BasicBlock* B, llvm::Function* F);

    void updateValueDependencies(llvm::BasicBlock* B);
    void updateCalledFunctionsList(llvm::BasicBlock* B);
    void updateReturnValueDependencies(llvm::BasicBlock* B);
    void updateOutArgumentDependencies(llvm::BasicBlock* B);
    void updateGlobals();
    void updateReferencedGlobals();
    void updateModifiedGlobals();
    DependencyAnaliser::ValueDependencies getBasicBlockPredecessorsDependencies(llvm::BasicBlock* B);
    DependencyAnaliser::ArgumentDependenciesMap getBasicBlockPredecessorsArguments(llvm::BasicBlock* B);
    const DependencyAnalysisResultT& getAnalysisResult(llvm::BasicBlock* B) const;

private:
    llvm::Function* m_F;
    llvm::AAResults& m_AAR;
    llvm::LoopInfo& m_LI;
    const llvm::PostDominatorTree& m_postDomTree;
    const VirtualCallSiteAnalysisResult& m_virtualCallsInfo;
    const IndirectCallSitesAnalysisResult& m_indirectCallsInfo;
    const FunctionAnalysisGetter& m_FAGetter;

    Arguments m_inputs;
    DependencyAnaliser::ValueDependencies m_valueDependencies; // all value dependencies
    DependencyAnaliser::ArgumentDependenciesMap m_outArgDependencies;
    DepInfo m_returnValueDependencies;
    FunctionArgumentsDependencies m_calledFunctionsInfo;
    FunctionGlobalsDependencies m_calledFunctionGlobalsInfo;
    FunctionSet m_calledFunctions;
    GlobalsSet m_referencedGlobals;
    GlobalsSet m_modifiedGlobals;
    bool m_globalsUpdated;

    std::unordered_map<llvm::BasicBlock*, DependencyAnalysisResultT> m_BBAnalysisResults;
    // LoopInfo will be invalidated after analisis, instead of keeping copy of it, keep this map.
    std::unordered_map<llvm::BasicBlock*, llvm::BasicBlock*> m_loopBlocks;

    llvm::Loop* m_currentLoop;
}; // class FunctionAnaliser::Impl


bool FunctionAnaliser::Impl::isInputDependent(llvm::Instruction* instr) const
{
    const auto& analysisRes = getAnalysisResult(instr->getParent());
    return analysisRes->isInputDependent(instr);
}

bool FunctionAnaliser::Impl::isInputIndependent(llvm::Instruction* instr) const
{
    const auto& analysisRes = getAnalysisResult(instr->getParent());
    return analysisRes->isInputIndependent(instr);
}

bool FunctionAnaliser::Impl::isOutArgInputIndependent(llvm::Argument* arg) const
{
    auto pos = m_outArgDependencies.find(arg);
    if (pos == m_outArgDependencies.end()) {
        return true;
    }
    return pos->second.isInputIndep();
}

DepInfo FunctionAnaliser::Impl::getOutArgDependencies(llvm::Argument* arg) const
{
    auto pos = m_outArgDependencies.find(arg);
    if (pos == m_outArgDependencies.end()) {
        return DepInfo();
    }
    return pos->second;
}

bool FunctionAnaliser::Impl::isReturnValueInputIndependent() const
{
    return m_returnValueDependencies.isInputIndep();
}

const DepInfo& FunctionAnaliser::Impl::getRetValueDependencies() const
{
    return m_returnValueDependencies;
}
 
bool FunctionAnaliser::Impl::hasGlobalVariableDepInfo(llvm::GlobalVariable* global) const
{
    auto& lastBB = m_F->back();
    const auto& pos = m_BBAnalysisResults.find(&lastBB);
    assert(pos != m_BBAnalysisResults.end());
    llvm::Value* val = llvm::dyn_cast<llvm::GlobalVariable>(global);
    assert(val != nullptr);
    return pos->second->hasValueDependencyInfo(val);
}

const DepInfo& FunctionAnaliser::Impl::getGlobalVariableDependencies(llvm::GlobalVariable* global) const
{
    auto& lastBB = m_F->back();
    const auto& pos = m_BBAnalysisResults.find(&lastBB);
    assert(pos != m_BBAnalysisResults.end());
    llvm::Value* val = llvm::dyn_cast<llvm::GlobalVariable>(global);
    assert(val != nullptr);
    return pos->second->getValueDependencyInfo(val);
}

DepInfo FunctionAnaliser::Impl::getDependencyInfoFromBlock(llvm::Value* val, llvm::BasicBlock* block) const
{
    if (val == nullptr || block == nullptr) {
        return DepInfo();
    }
    if (auto global = llvm::dyn_cast<llvm::GlobalVariable>(val)) {
        return getGlobalVariableDependencies(global);
    }
    const auto& analysisRes = getAnalysisResult(block);
    if (analysisRes->hasValueDependencyInfo(val)) {
        return analysisRes->getValueDependencyInfo(val);
    }
    auto instr = llvm::dyn_cast<llvm::Instruction>(val);
    assert(instr != nullptr);
    if (instr->getParent() == block) {
        return analysisRes->getInstructionDependencies(instr);
    }
    return DepInfo();
}

const DependencyAnaliser::ArgumentDependenciesMap&
FunctionAnaliser::Impl::getCallArgumentInfo(llvm::Function* F) const
{
    auto pos = m_calledFunctionsInfo.find(F);
    if (pos == m_calledFunctionsInfo.end()) {
        const_cast<Impl*>(this)->updateFunctionCallInfo(F);
    }
    pos = m_calledFunctionsInfo.find(F);
    return pos->second;
}

FunctionCallDepInfo FunctionAnaliser::Impl::getFunctionCallDepInfo(llvm::Function* F) const
{
    assert(m_calledFunctions.find(F) != m_calledFunctions.end());
    FunctionCallDepInfo callDepInfo(*F);
    for (const auto& result : m_BBAnalysisResults) {
        if (result.second->hasFunctionCallInfo(F)) {
            const auto& info = result.second->getFunctionCallInfo(F);
            callDepInfo.addDepInfo(info);
        }
    }
    return callDepInfo;
}

const DependencyAnaliser::GlobalVariableDependencyMap&
FunctionAnaliser::Impl::getCallGlobalsInfo(llvm::Function* F) const
{
    auto pos = m_calledFunctionGlobalsInfo.find(F);
    if (pos == m_calledFunctionGlobalsInfo.end()) {
        const_cast<Impl*>(this)->updateFunctionCallGlobalsInfo(F);
    }
    pos = m_calledFunctionGlobalsInfo.find(F);
    return pos->second;
}

const GlobalsSet& FunctionAnaliser::Impl::getReferencedGlobals() const
{
    if (!m_globalsUpdated) {
        assert(m_referencedGlobals.empty());
        const_cast<Impl*>(this)->updateGlobals();
    }
    return m_referencedGlobals;
}

const GlobalsSet& FunctionAnaliser::Impl::getModifiedGlobals() const
{
    if (!m_globalsUpdated) {
        assert(m_modifiedGlobals.empty());
        const_cast<Impl*>(this)->updateGlobals();
    }
    return m_modifiedGlobals;
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
            const auto& depInfo = getBasicBlockPredecessorInstructionsDeps(bb);
            LoopAnalysisResult* loopA = new LoopAnalysisResult(m_F, m_AAR,
                                                               m_postDomTree,
                                                               m_virtualCallsInfo,
                                                               m_indirectCallsInfo,
                                                               m_inputs,
                                                               m_FAGetter,
                                                               *m_currentLoop,
                                                               m_LI);
            if (depInfo.isDefined()) {
                loopA->setLoopDependencies(depInfo);
            }
            m_BBAnalysisResults[bb].reset(loopA);
        } else if (auto loop = m_LI.getLoopFor(bb)) {
            // there are cases when blocks of two not nested loops are processed in mixed order
            if (!m_currentLoop->contains(loop)) {
                m_currentLoop = loop;
            }
            m_loopBlocks[bb] = m_currentLoop->getHeader();
            continue;
        } else {
            //m_currentLoop = nullptr;
            m_BBAnalysisResults[bb] = createBasicBlockAnalysisResult(bb);
        }
        llvm::dbgs() << "BB " << bb->getName() << "\n";
        m_BBAnalysisResults[bb]->setInitialValueDependencies(getBasicBlockPredecessorsDependencies(bb));
        m_BBAnalysisResults[bb]->setOutArguments(getBasicBlockPredecessorsArguments(bb));
        m_BBAnalysisResults[bb]->gatherResults();

        updateValueDependencies(bb);
        updateCalledFunctionsList(bb);
        updateReturnValueDependencies(bb);
        updateOutArgumentDependencies(bb);
    }
    m_inputs.clear();
}

void FunctionAnaliser::Impl::finalizeArguments(const ArgumentDependenciesMap& dependentArgs)
{
    //llvm::dbgs() << "finalizing with dependencies\n";
    //for (const auto& arg : dependentArgs) {
    //    llvm::dbgs() << *arg.first << "     " << arg.second.getDependencyName() << "\n";
    //}

    m_calledFunctionsInfo.clear();
    m_calledFunctionGlobalsInfo.clear();
    for (auto& item : m_BBAnalysisResults) {
        item.second->finalizeResults(dependentArgs);
        updateFunctionCallsInfo(item.first);
        updateFunctionCallsGlobalsInfo(item.first);
    }
}

void FunctionAnaliser::Impl::finalizeGlobals(const GlobalVariableDependencyMap& globalsDeps)
{
    for (auto& item : m_BBAnalysisResults) {
        item.second->finalizeGlobals(globalsDeps);
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
    if (depInfo.isInputDep()) {
        return DependencyAnalysisResultT(
                    new InputDependentBasicBlockAnaliser(m_F, m_AAR, m_virtualCallsInfo, m_indirectCallsInfo, m_inputs, m_FAGetter, B));
    } else if (depInfo.isInputArgumentDep()) {
        return DependencyAnalysisResultT(
           new NonDeterministicBasicBlockAnaliser(m_F, m_AAR, m_virtualCallsInfo, m_indirectCallsInfo, m_inputs, m_FAGetter, B, depInfo));
    }
    return DependencyAnalysisResultT(
            new BasicBlockAnalysisResult(m_F, m_AAR, m_virtualCallsInfo, m_indirectCallsInfo, m_inputs, m_FAGetter, B));
}

DepInfo FunctionAnaliser::Impl::getBasicBlockPredecessorInstructionsDeps(llvm::BasicBlock* B) const
{
    DepInfo dep(DepInfo::DepInfo::INPUT_INDEP);
    bool postdominates_all_predecessors = true;
    const auto& b_node = m_postDomTree[B];
    auto pred = pred_begin(B);
    while (pred != pred_end(B)) {
        auto pb = *pred;
        const auto& termInstr = pb->getTerminator();
        if (termInstr == nullptr) {
            dep.setDependency(DepInfo::DepInfo::INPUT_ARGDEP);
            break;
        }
        // predecessor is in loop
        // We assume loops are not inifinite, and all exit blocks lead to the same block, thus this basic block will be reached no mater if loop condition is input dep or not.
        if (m_LI.getLoopFor(pb) != nullptr) {
            ++pred;
            continue;
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
            ++pred;
            // We assume loops are not inifinite, this this basic block will be reached no mater if loop condition is input dep or not.
            continue;
        }
        assert(pos != m_BBAnalysisResults.end());
        if (pos != m_BBAnalysisResults.end()) {
            dep.mergeDependencies(pos->second->getInstructionDependencies(termInstr));
        }
        auto pred_node = m_postDomTree[*pred];
        postdominates_all_predecessors &= m_postDomTree.dominates(b_node, pred_node);
        ++pred;
    }
    // if block postdominates all its predecessors, it will be reached independent of predecessors.
    if (postdominates_all_predecessors) {
        return DepInfo(DepInfo::INPUT_INDEP);
    }
    return dep;
}

void FunctionAnaliser::Impl::updateFunctionCallInfo(llvm::Function* F)
{
    for (const auto& B : m_BBAnalysisResults) {
        updateFunctionCallInfo(B.first, F);
    }
}

void FunctionAnaliser::Impl::updateFunctionCallGlobalsInfo(llvm::Function* F)
{
    for (const auto& B : m_BBAnalysisResults) {
        updateFunctionCallGlobalsInfo(B.first, F);
    }
}

void FunctionAnaliser::Impl::updateFunctionCallsInfo(llvm::BasicBlock* B)
{
    const auto& info = m_BBAnalysisResults[B]->getFunctionsCallInfo();
    for (const auto& item : info) {
        auto argDeps = item.second.getMergedArgumentDependencies();
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
    auto argDeps = info.getMergedArgumentDependencies();
    auto res = m_calledFunctionsInfo.insert(std::make_pair(F, argDeps));
    if (!res.second) {
        for (auto& deps : argDeps) {
            auto r = res.first->second.insert(deps);
            if (!r.second) {
                r.first->second.mergeDependencies(argDeps[r.first->first]);
            }
        }
    }
}

void FunctionAnaliser::Impl::updateFunctionCallsGlobalsInfo(llvm::BasicBlock* B)
{
    const auto& info = m_BBAnalysisResults[B]->getFunctionsCallInfo();
    for (const auto& item : info) {
        auto globalsDeps = item.second.getMergedGlobalsDependencies();
        auto res = m_calledFunctionGlobalsInfo.insert(std::make_pair(item.first, globalsDeps));
        if (!res.second) {
            for (auto& deps : res.first->second) {
                deps.second.mergeDependencies(globalsDeps[deps.first]);
            }
        }
    }
}

void FunctionAnaliser::Impl::updateFunctionCallGlobalsInfo(llvm::BasicBlock* B, llvm::Function* F)
{
    const auto& BA = m_BBAnalysisResults[B];
    if (!BA->hasFunctionCallInfo(F)) {
        return;
    }
    const auto& info = BA->getFunctionCallInfo(F);
    auto globalsDeps = info.getMergedGlobalsDependencies();
    auto res = m_calledFunctionGlobalsInfo.insert(std::make_pair(F, globalsDeps));
    if (!res.second) {
        for (auto& deps : globalsDeps) {
            auto r = res.first->second.insert(deps);
            if (!r.second) {
                r.first->second.mergeDependencies(globalsDeps[r.first->first]);
            }
        }
    }
}

void FunctionAnaliser::Impl::updateValueDependencies(llvm::BasicBlock* B)
{
    // Note1: entry basic block will have all values in its value dependencies list, as all values are allocated in entry block,
    // hence added to it's value dependencies list
    // Thus m_valueDependencies will always contain full information about values in function.
    // Note2: This is not necessarily valid information, e.g. for branches, it will contain the values from the block analyzed later,
    // but it is then fixed in getBasicBlockPredecessorsDependencies function, which merges dependencies of branch blocks
    // each block will get only values not present in its' predecessors from this set
    const auto& block_deps = m_BBAnalysisResults[B]->getValuesDependencies();
    for (const auto& val : block_deps) {
        m_valueDependencies[val.first] = val.second;
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

void FunctionAnaliser::Impl::updateGlobals()
{
    updateReferencedGlobals();
    updateModifiedGlobals();
    m_globalsUpdated = true;
}

void FunctionAnaliser::Impl::updateReferencedGlobals()
{
    for (const auto& BB : m_BBAnalysisResults) {
        const auto& refGlobals = BB.second->getReferencedGlobals();
        m_referencedGlobals.insert(refGlobals.begin(), refGlobals.end());
    }
}

void FunctionAnaliser::Impl::updateModifiedGlobals()
{
    for (const auto& BB : m_BBAnalysisResults) {
        const auto& modGlobals = BB.second->getModifiedGlobals();
        m_modifiedGlobals.insert(modGlobals.begin(), modGlobals.end());
    }
}

DependencyAnaliser::ValueDependencies
FunctionAnaliser::Impl::getBasicBlockPredecessorsDependencies(llvm::BasicBlock* B)
{
    DependencyAnaliser::ValueDependencies deps;
    auto pred = pred_begin(B);
    while (pred != pred_end(B)) {
        auto pos = m_BBAnalysisResults.find(*pred);
        if (pos == m_BBAnalysisResults.end()) {
            //assert(m_LI.getLoopFor(*pred) != nullptr);
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
            auto res = deps.insert(dep);
            if (!res.second) {
                res.first->second.mergeDependencies(dep.second);
            }
        }
        ++pred;
    }
    // Note: values which have been added from predecessors won't change here
    deps.insert(m_valueDependencies.begin(), m_valueDependencies.end());
    return deps;
}

DependencyAnaliser::ArgumentDependenciesMap
FunctionAnaliser::Impl::getBasicBlockPredecessorsArguments(llvm::BasicBlock* B)
{
    auto pred = pred_begin(B);
    // entry block
    if (pred == pred_end(B)) {
        return m_outArgDependencies;
    }
    DependencyAnaliser::ArgumentDependenciesMap deps;
    while (pred != pred_end(B)) {
        auto pos = m_BBAnalysisResults.find(*pred);
        if (pos == m_BBAnalysisResults.end()) {
            //assert(m_LI.getLoopFor(*pred) != nullptr);
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
            auto res = deps.insert(dep);
            if (!res.second) {
                res.first->second.mergeDependencies(dep.second);
            }
        }
        ++pred;
    }
    return deps;
}

const FunctionAnaliser::Impl::DependencyAnalysisResultT& FunctionAnaliser::Impl::getAnalysisResult(llvm::BasicBlock* bb) const
{
    assert(bb->getParent() == m_F);
    auto looppos = m_loopBlocks.find(bb);
    if (looppos != m_loopBlocks.end()) {
        bb = looppos->second;
    }
    auto pos = m_BBAnalysisResults.find(bb);
    assert(pos != m_BBAnalysisResults.end());
    return pos->second;
}

FunctionAnaliser::FunctionAnaliser(llvm::Function* F,
                                   llvm::AAResults& AAR,
                                   llvm::LoopInfo& LI,
                                   const llvm::PostDominatorTree& PDom,
                                   const VirtualCallSiteAnalysisResult& VCAR,
                                   const IndirectCallSitesAnalysisResult& ICAR,
                                   const FunctionAnalysisGetter& getter)
    : m_analiser(new Impl(F, AAR, LI, PDom, VCAR, ICAR, getter))
{
}

void FunctionAnaliser::setFunction(llvm::Function* F)
{
    m_analiser->setFunction(F);
}

void FunctionAnaliser::analize()
{
    m_analiser->analize();
}

void FunctionAnaliser::finalizeArguments(const DependencyAnaliser::ArgumentDependenciesMap& dependentArgNos)
{
    m_analiser->finalizeArguments(dependentArgNos);
}

void FunctionAnaliser::finalizeGlobals(const DependencyAnaliser::GlobalVariableDependencyMap& globalsDeps)
{
    m_analiser->finalizeGlobals(globalsDeps);
}

FunctionSet FunctionAnaliser::getCallSitesData() const
{
    return m_analiser->getCallSitesData();
}

const DependencyAnaliser::ArgumentDependenciesMap&
FunctionAnaliser::getCallArgumentInfo(llvm::Function* F) const
{
    return m_analiser->getCallArgumentInfo(F);
}

FunctionCallDepInfo FunctionAnaliser::getFunctionCallDepInfo(llvm::Function* F) const
{
    return m_analiser->getFunctionCallDepInfo(F);
}

DependencyAnaliser::GlobalVariableDependencyMap
FunctionAnaliser::getCallGlobalsInfo(llvm::Function* F) const
{
    return m_analiser->getCallGlobalsInfo(F);
}

bool FunctionAnaliser::isInputDependent(llvm::Instruction* instr) const
{
    return m_analiser->isInputDependent(instr);
}

bool FunctionAnaliser::isInputDependent(const llvm::Instruction* instr) const
{
    return m_analiser->isInputDependent(const_cast<llvm::Instruction*>(instr));
}

bool FunctionAnaliser::isInputIndependent(llvm::Instruction* instr) const
{
    return m_analiser->isInputIndependent(instr);
}

bool FunctionAnaliser::isInputIndependent(const llvm::Instruction* instr) const
{
    return m_analiser->isInputIndependent(const_cast<llvm::Instruction*>(instr));
}

bool FunctionAnaliser::isOutArgInputIndependent(llvm::Argument* arg) const
{
    return m_analiser->isOutArgInputIndependent(arg);
}

DepInfo FunctionAnaliser::getOutArgDependencies(llvm::Argument* arg) const
{
    return m_analiser->getOutArgDependencies(arg);
}

bool FunctionAnaliser::isReturnValueInputIndependent() const
{
    return m_analiser->isReturnValueInputIndependent();
}

const DepInfo& FunctionAnaliser::getRetValueDependencies() const
{
    return m_analiser->getRetValueDependencies();
}

bool FunctionAnaliser::hasGlobalVariableDepInfo(llvm::GlobalVariable* global) const
{
    return m_analiser->hasGlobalVariableDepInfo(global);
}

const DepInfo& FunctionAnaliser::getGlobalVariableDependencies(llvm::GlobalVariable* global) const
{
    return m_analiser->getGlobalVariableDependencies(global);
}

DepInfo FunctionAnaliser::getDependencyInfoFromBlock(llvm::Value* val, llvm::BasicBlock* block) const
{
    return m_analiser->getDependencyInfoFromBlock(val, block);
}

const GlobalsSet& FunctionAnaliser::getReferencedGlobals() const
{
    return m_analiser->getReferencedGlobals();
}

const GlobalsSet& FunctionAnaliser::getModifiedGlobals() const
{
    return m_analiser->getModifiedGlobals();
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

