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
    using FunctionArgumentsDependencies = DependencyAnaliser::FunctionArgumentsDependencies;
    using PredValDeps = DependencyAnalysisResult::InitialValueDpendencies;
    using PredArgDeps = DependencyAnalysisResult::InitialArgumentDependencies;
    using DependencyAnalysisResultT = std::unique_ptr<DependencyAnalysisResult>;

public:
    bool isInputDependent(llvm::Instruction* instr) const;
    bool isOutArgInputDependent(llvm::Argument* arg) const;
    ArgumentSet getOutArgDependencies(llvm::Argument* arg) const;
    bool isReturnValueInputDependent() const;
    ArgumentSet getRetValueDependencies() const;

    void analize();
    void finalize(const ArgumentDependenciesMap& dependentArgNos);
    void dump() const;

    // Returns collected data for function calls in this function
    FunctionArgumentsDependencies getCallSitesData() const
    {
        return m_calledFunctionsInfo;
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
    void updateFunctionCallInfo(llvm::BasicBlock* B);
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
    return pos->second.isInputDep();
}

ArgumentSet FunctionAnaliser::Impl::getOutArgDependencies(llvm::Argument* arg) const
{
    auto pos = m_outArgDependencies.find(arg);
    if (pos == m_outArgDependencies.end()) {
        return ArgumentSet();
    }
    return pos->second.getArgumentDependencies();
}

bool FunctionAnaliser::Impl::isReturnValueInputDependent() const
{
    return m_returnValueDependencies.isInputDep();
}

ArgumentSet FunctionAnaliser::Impl::getRetValueDependencies() const
{
    return m_returnValueDependencies.getArgumentDependencies();
}

void FunctionAnaliser::Impl::analize()
{
    //llvm::dbgs() << "In function " << m_F->getName() << "\n";
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
            m_BBAnalysisResults[bb].reset(new LoopAnalysisResult(m_F, m_AAR, m_inputs, m_FAGetter, *m_currentLoop));
        } else if (auto loop = m_LI.getLoopFor(bb)) {
            m_loopBlocks[bb] = m_currentLoop->getHeader();
            //llvm::dbgs() << "skip loop block " << bb->getName() << "\n";
            continue;
        } else {
            //m_currentLoop = nullptr;
            m_BBAnalysisResults[bb] = createBasicBlockAnalysisResult(bb);
        }
        m_BBAnalysisResults[bb]->setInitialValueDependencies(getBasicBlockPredecessorsDependencies(bb));
        m_BBAnalysisResults[bb]->setOutArguments(getBasicBlockPredecessorsArguments(bb));
        m_BBAnalysisResults[bb]->gatherResults();

        updateFunctionCallInfo(bb);
        updateReturnValueDependencies(bb);
        updateOutArgumentDependencies(bb);
    }
    m_inputs.clear();
}

void FunctionAnaliser::Impl::finalize(const ArgumentDependenciesMap& dependentArgs)
{
    for (auto& item : m_BBAnalysisResults) {
        item.second->finalizeResults(dependentArgs);
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
//    for (auto& item : m_BBAnalysisResults) {
//        item.second->dumpResults();
//    }
}

void FunctionAnaliser::Impl::collectArguments()
{
    auto& arguments = m_F->getArgumentList();
    std::for_each(arguments.begin(), arguments.end(),
            [this] (llvm::Argument& arg) {
                this->m_inputs.push_back(&arg);
                llvm::Value* val = llvm::dyn_cast<llvm::Value>(&arg);
                if (val->getType()->isPointerTy()) {
                    m_outArgDependencies[&arg] = DepInfo(DepInfo::INPUT_DEP, ArgumentSet{&arg});
                }
            });
}

FunctionAnaliser::Impl::DependencyAnalysisResultT
FunctionAnaliser::Impl::createBasicBlockAnalysisResult(llvm::BasicBlock* B)
{
    const auto& depInfo = getBasicBlockPredecessorInstructionsDeps(B);
    if (depInfo.isInputDep()) {
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
            dep.setDependency(DepInfo::DepInfo::INPUT_DEP);
            break;
        }

        auto pos = m_BBAnalysisResults.find(pb);
        if (pos == m_BBAnalysisResults.end()) {
            assert(m_LI.getLoopFor(pb) != nullptr);
            //assert(m_currentLoop != nullptr);
            //assert(*pred == m_currentLoop->getLoopLatch());
            ++pred;
            continue;
        }
        assert(pos != m_BBAnalysisResults.end());
        if (pos->second->isInputDependent(termInstr)) {
            dep.setDependency(DepInfo::INPUT_DEP);
            dep.mergeDependencies(pos->second->getInstructionDependencies(termInstr));
        }
        ++pred;
    }
    return dep;
}

void FunctionAnaliser::Impl::updateFunctionCallInfo(llvm::BasicBlock* B)
{
    const auto& info = m_BBAnalysisResults[B]->getFunctionsCallInfo();
    for (const auto& item : info) {
        auto res = m_calledFunctionsInfo.insert(item);
        if (res.second) {
            continue;
        }
        auto& depMap = res.first->second;
        for (const auto& depItem : item.second) {
            depMap[depItem.first].mergeDependencies(depItem.second);
        }
    }
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
            ++pred;
            continue;
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
            ++pred;
            continue;
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

DependencyAnaliser::FunctionArgumentsDependencies FunctionAnaliser::getCallSitesData() const
{
    return m_analiser->getCallSitesData();
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

ArgumentSet FunctionAnaliser::getOutArgDependencies(llvm::Argument* arg) const
{
    return m_analiser->getOutArgDependencies(arg);
}

bool FunctionAnaliser::isReturnValueInputDependent() const
{
    return m_analiser->isReturnValueInputDependent();
}

ArgumentSet FunctionAnaliser::getRetValueDependencies() const
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

