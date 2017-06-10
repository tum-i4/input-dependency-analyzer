#include "LoopAnalysisResult.h"

#include "ReflectingBasicBlockAnaliser.h"
#include "InputDependentBasicBlockAnaliser.h"
#include "NonDeterministicReflectingBasicBlockAnaliser.h"
#include "VirtualCallSitesAnalysis.h"
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

namespace {
class LoopTraversalPathCreator
{
public:
    using LoopPathType = std::list<llvm::BasicBlock*>;

public:
    LoopTraversalPathCreator(llvm::LoopInfo& LI,
                             llvm::Loop& L);

public:
    LoopPathType& getPath()
    {
        return m_path;
    }
public:
    void construct();

private:
    bool add_predecessors(llvm::BasicBlock* block, LoopPathType& blocks);
    void add_successors(llvm::BasicBlock* block,
                        const std::unordered_set<llvm::BasicBlock*>& seen_blocks,
                        LoopPathType& blocks);
    void add_to_path(llvm::BasicBlock* block);

private:
    llvm::LoopInfo& m_LI;
    llvm::Loop& m_L;
    std::unordered_set<llvm::BasicBlock*> m_uniquify_map;
    LoopPathType m_path;
};

LoopTraversalPathCreator::LoopTraversalPathCreator(llvm::LoopInfo& LI, llvm::Loop& L)
    : m_LI(LI)
    , m_L(L)
{
}

void LoopTraversalPathCreator::construct()
{
    std::list<llvm::BasicBlock*> blocks;
    std::unordered_set<llvm::BasicBlock*> seen_blocks;

    blocks.push_back(m_L.getHeader());
    while (!blocks.empty()) {
        auto block = blocks.back();
        if (m_uniquify_map.find(block) != m_uniquify_map.end()) {
            blocks.pop_back();
            continue;
        }
        // if seen assume all predecessors has been added
        if (seen_blocks.find(block) == seen_blocks.end()) {
            bool preds_added = add_predecessors(block, blocks);
            if (!preds_added) {
                seen_blocks.insert(block);
                continue;
            }
        }
        add_to_path(block);
        blocks.pop_back();
        add_successors(block, seen_blocks, blocks);
    }

}

bool LoopTraversalPathCreator::add_predecessors(llvm::BasicBlock* block, LoopPathType& blocks)
{
    auto pred = pred_begin(block);
    bool preds_added = true;
    // add notion of seen blocks, not to traverse pred's second time
    while (pred != pred_end(block)) {
        auto pred_loop = m_LI.getLoopFor(*pred);
        if (pred_loop == nullptr) {
            ++pred;
            continue;
        }
        if (m_uniquify_map.find(*pred) != m_uniquify_map.end()) {
            ++pred;
            continue;
        }
        if (pred_loop != &m_L) {
            // predecessor is in outer loop, outer loops are already processed
            if (pred_loop->contains(&m_L)) {
                ++pred;
                assert(m_L.getHeader() == block);
                continue;
            }
            auto pred_loop_head = pred_loop->getHeader();
            if (m_uniquify_map.find(pred_loop_head) != m_uniquify_map.end()) {
                ++pred;
                continue;
            } else {
                preds_added = false;
                blocks.push_back(pred_loop_head);
            }
        } else {
            preds_added = false;
            blocks.push_back(*pred);
        }
        ++pred;
    }
    return preds_added;
}

void LoopTraversalPathCreator::add_successors(llvm::BasicBlock* block,
                                              const std::unordered_set<llvm::BasicBlock*>& seen_blocks,
                                              LoopPathType& blocks)
{
    auto succ = succ_begin(block);
    while (succ != succ_end(block)) {
        if (seen_blocks.find(*succ) != seen_blocks.end()) {
            ++succ;
            continue;
        }
        if (m_uniquify_map.find(*succ) != m_uniquify_map.end()) {
            ++succ;
            continue;
        }
        auto succ_loop = m_LI.getLoopFor(*succ);
        if (succ_loop == nullptr) {
            // getLoopFor should be constant time, as denseMap is implemented as hash table 
            // is_loopExiting is not constant
            //assert(m_LI.getLoopFor(block)->isLoopExiting(block));
            ++succ;
            continue;
        }
        if (succ_loop != &m_L) {
            if (succ_loop->getHeader() != *succ) {
                if (succ_loop->contains(&m_L)) {
                    // is_loopExiting is not constant
                    //assert(m_L.isLoopExiting(block));
                } else if (m_L.contains(succ_loop)) {
                    // assert block is subloop head
                    assert(succ_loop->getHeader() == block);
                }
                ++succ;
                continue;
            }
        }
        blocks.push_back(*succ);
        ++succ;
    }
}


void LoopTraversalPathCreator::add_to_path(llvm::BasicBlock* block)
{
    auto block_loop = m_LI.getLoopFor(block);
    assert(block_loop != nullptr);
    // comparing header is cheaper than isLoopHeader
    if (block_loop != &m_L && block_loop->getHeader() != block) {
        // assume the header of loop has been processed, hence is in m_uniquify_map
        assert(m_uniquify_map.find(block_loop->getHeader()) != m_uniquify_map.end());
        return;
    }
    m_path.push_back(block);
    m_uniquify_map.insert(block);
}

}

LoopAnalysisResult::LoopAnalysisResult(llvm::Function* F,
                                       llvm::AAResults& AAR,
                                       const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                                       const Arguments& inputs,
                                       const FunctionAnalysisGetter& Fgetter,
                                       llvm::Loop& L,
                                       llvm::LoopInfo& LI)
                                : m_F(F)
                                , m_AAR(AAR)
                                , m_virtualCallsInfo(virtualCallsInfo)
                                , m_inputs(inputs)
                                , m_FAG(Fgetter)
                                , m_L(L)
                                , m_LI(LI)
                                , m_globalsUpdated(false)
                                , m_isReflected(false)
{
    using BlocksVector = llvm::SmallVector<llvm::BasicBlock*, 10>;
    BlocksVector loop_latches;
    m_L.getLoopLatches(loop_latches);
    m_latches.insert(loop_latches.begin(), loop_latches.end());
    loop_latches.clear();
}

void LoopAnalysisResult::gatherResults()
{
    LoopTraversalPathCreator pathCreator(m_LI, m_L);
    pathCreator.construct();
    auto blocks = pathCreator.getPath();

    //llvm::dbgs() << "Loop will be traversed in order\n";
    //for (const auto& block : blocks) {
    //    llvm::dbgs() << block->getName() << "\n";
    //}

    bool is_input_dep = false;
    for (const auto& B : blocks) {
        updateLoopDependecies(B);
        is_input_dep = checkForLoopDependencies(m_initialDependencies);
        if (is_input_dep) {
            break;
        }
        m_BBAnalisers[B] = createDependencyAnaliser(B);
        auto& analiser = m_BBAnalisers[B];
        analiser->setInitialValueDependencies(getBasicBlockPredecessorsDependencies(B));
        analiser->setOutArguments(getBasicBlockPredecessorsArguments(B));
        analiser->gatherResults();
        is_input_dep = checkForLoopDependencies(B);
        if (is_input_dep) {
            break;
        }
        //m_BBAnalisers[B]->dumpResults();
    }
    if (is_input_dep) {
        for (const auto& B : blocks) {
            auto Bpos = m_BBAnalisers.find(B);
            if (Bpos != m_BBAnalisers.end()) {
                Bpos->second->markAllInputDependent();
            } else {
                m_BBAnalisers[B] = createInputDependentAnaliser(B);
                //analiser->setInitialValueDependencies(getBasicBlockPredecessorsDependencies(B));
                //analiser->setOutArguments(getBasicBlockPredecessorsArguments(B));
                m_BBAnalisers[B]->gatherResults();
            }
        }
    } else {
        reflect();
    }
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

void LoopAnalysisResult::finalizeGlobals(const DependencyAnaliser::GlobalVariableDependencyMap& globalsDeps)
{
    for (auto& item : m_BBAnalisers) {
        item.second->finalizeGlobals(globalsDeps);
    }
    updateGlobals();
}

void LoopAnalysisResult::dumpResults() const
{
    for (const auto& item : m_BBAnalisers) {
        item.second->dumpResults();
    }
}

void LoopAnalysisResult::setLoopDependencies(const DepInfo& loopDeps)
{
    m_loopDependencies = loopDeps;
}

void LoopAnalysisResult::setInitialValueDependencies(
            const DependencyAnaliser::ValueDependencies& valueDependencies)
{
    m_initialDependencies = valueDependencies;
}

void LoopAnalysisResult::setOutArguments(const DependencyAnaliser::ArgumentDependenciesMap& outArgs)
{
    m_outArgDependencies = outArgs;
}

bool LoopAnalysisResult::isInputDependent(llvm::Instruction* instr) const
{
    auto parentBB = instr->getParent();
    auto pos = m_BBAnalisers.find(parentBB);
    if (pos != m_BBAnalisers.end()) {
        return pos->second->isInputDependent(instr);
    }

    // loop info might be invalidated here. lookup in map
    auto loop_head_pos = m_loopBlocks.find(parentBB);
    assert(loop_head_pos != m_loopBlocks.end());
    auto loop_pos = m_BBAnalisers.find(loop_head_pos->second);
    assert(loop_pos != m_BBAnalisers.end());
    return loop_pos->second->isInputDependent(instr);
}

bool LoopAnalysisResult::isInputIndependent(llvm::Instruction* instr) const
{
    auto parentBB = instr->getParent();
    auto loop = m_LI.getLoopFor(parentBB);
    if (loop == &m_L) {
        auto pos = m_BBAnalisers.find(parentBB);
        assert(pos != m_BBAnalisers.end());
        return pos->second->isInputIndependent(instr);
    }
    //TODO: implement this part. would need to check the depth for a  loop and redirect call to correct loopAnalysisResult
    return false;
}

bool LoopAnalysisResult::hasValueDependencyInfo(llvm::Value* val) const
{
    auto pos = m_valueDependencies.find(val);
    if (pos != m_valueDependencies.end()) {
        return true;
    }
    return m_initialDependencies.find(val) != m_initialDependencies.end();
}

const DepInfo& LoopAnalysisResult::getValueDependencyInfo(llvm::Value* val)
{
    auto pos = m_valueDependencies.find(val);
    if (pos != m_valueDependencies.end()) {
        return pos->second;
    }
    auto initial_val_pos = m_initialDependencies.find(val);
    assert(initial_val_pos != m_initialDependencies.end());
    // add referenced value
    m_valueDependencies[val] = initial_val_pos->second;
    return initial_val_pos->second;
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

const GlobalsSet& LoopAnalysisResult::getReferencedGlobals() const
{
    if (!m_globalsUpdated) {
        assert(m_referencedGlobals.empty());
        const_cast<LoopAnalysisResult*>(this)->updateGlobals();
    }
    return m_referencedGlobals;
}

const GlobalsSet& LoopAnalysisResult::getModifiedGlobals() const
{
    if (!m_globalsUpdated) {
        assert(m_modifiedGlobals.empty());
        const_cast<LoopAnalysisResult*>(this)->updateGlobals();
    }
    return m_modifiedGlobals;
}

void LoopAnalysisResult::markAllInputDependent()
{
    for (auto& bbAnaliser : m_BBAnalisers) {
        bbAnaliser.second->markAllInputDependent();
    }
}

void LoopAnalysisResult::reflect(const DependencyAnaliser::ValueDependencies& dependencies, const DepInfo& mandatory_deps)
{
    if (checkForLoopDependencies(dependencies)) {
        markAllInputDependent();
        return;
    }
    for (auto& analiser : m_BBAnalisers) {
        analiser.second->reflect(dependencies, mandatory_deps);
    }
}

bool LoopAnalysisResult::isSpecialLoopBlock(llvm::BasicBlock* B) const
{
    return m_L.getHeader() == B || m_latches.find(B) != m_latches.end() || m_L.isLoopExiting(B);
}

DependencyAnaliser::ValueDependencies LoopAnalysisResult::getBasicBlockPredecessorsDependencies(llvm::BasicBlock* B)
{
    // predecessor is outside of the loop
    if (m_L.getHeader() == B) {
        return m_initialDependencies;
    }
    // add only values modified (or referenced) in predecessor blocks
    DependencyAnaliser::ValueDependencies deps;
    auto pred = pred_begin(B);
    while (pred != pred_end(B)) {
        auto pos = m_BBAnalisers.find(*pred);
        // predecessor is latch
        if (pos == m_BBAnalisers.end()) {
            ++pred;
            continue;
        }
        assert(pos != m_BBAnalisers.end());
        const auto& valueDeps = pos->second->getValuesDependencies();
        for (auto& dep : valueDeps) {
            auto pos = deps.insert(dep);
            if (!pos.second) {
                pos.first->second.mergeDependencies(dep.second);
            }
        }
        ++pred;
    }
    // add initial values. Note values which have been added from prdecessors are not going to be changed
    deps.insert(m_initialDependencies.begin(), m_initialDependencies.end());
    return deps;
}

DependencyAnaliser::ArgumentDependenciesMap LoopAnalysisResult::getBasicBlockPredecessorsArguments(llvm::BasicBlock* B)
{
    DependencyAnaliser::ArgumentDependenciesMap deps;
    auto pred = pred_begin(B);
    while (pred != pred_end(B)) {
        if (!m_L.contains(*pred)) {
            for (const auto& item : m_outArgDependencies) {
                auto pos = deps.insert(item);
                if (!pos.second) {
                    pos.first->second.mergeDependencies(item.second);
                }
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
            auto pos = deps.insert(dep);
            if (!pos.second) {
                pos.first->second.mergeDependencies(dep.second);
            }
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
            res.first->second.addDepInfo(callItem.second);
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
            res.first->second.addDepInfo(callInfo);
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
    for (const auto& item : valuesDeps) {
        auto res = m_valueDependencies.insert(item);
        if (!res.second) {
            res.first->second.mergeDependencies(item.second);
        }
    }
}

void LoopAnalysisResult::updateGlobals()
{
    updateReferencedGlobals();
    updateModifiedGlobals();
    m_globalsUpdated = true;
}

void LoopAnalysisResult::updateReferencedGlobals()
{
    for (const auto& item : m_BBAnalisers) {
        const auto& refGlobals = item.second->getReferencedGlobals();
        m_referencedGlobals.insert(refGlobals.begin(), refGlobals.end());
    }
}

void LoopAnalysisResult::updateModifiedGlobals()
{
    for (const auto& item : m_BBAnalisers) {
        const auto& modGlobals = item.second->getModifiedGlobals();
        m_modifiedGlobals.insert(modGlobals.begin(), modGlobals.end());
    }
}

void LoopAnalysisResult::reflect()
{
    DependencyAnaliser::ValueDependencies valueDependencies;
    for (const auto& latch : m_latches) {
        auto valueDeps = m_BBAnalisers[latch]->getValuesDependencies();
        for (const auto& dep : valueDeps) {
            auto res = valueDependencies.insert(dep);
            if (!res.second) {
                res.first->second.mergeDependencies(dep.second);
            }
        }
    }
    reflect(valueDependencies, m_loopDependencies);
}

LoopAnalysisResult::ReflectingDependencyAnaliserT LoopAnalysisResult::createDependencyAnaliser(llvm::BasicBlock* B)
{
    auto depInfo = getBasicBlockDeps(B);
    auto block_loop = m_LI.getLoopFor(B);
    if (block_loop != &m_L) {
        LoopAnalysisResult* loopAnalysisResult = new LoopAnalysisResult(m_F, m_AAR, m_virtualCallsInfo,
                                                                        m_inputs, m_FAG, *block_loop, m_LI);
        loopAnalysisResult->setLoopDependencies(depInfo);
        collectLoopBlocks(block_loop);
        return ReflectingDependencyAnaliserT(loopAnalysisResult);
    }
    // loop argument dependencies will also become basic blocks argument dependencies.
    // this should not make runtime worse as argument dependencies does not affect reflection algorithm. 
    depInfo.mergeDependencies(m_loopDependencies.getArgumentDependencies());
    if (depInfo.isInputArgumentDep()) {
        depInfo.mergeDependency(DepInfo::INPUT_ARGDEP);
    }
    if (depInfo.isInputIndep()) {
        return ReflectingDependencyAnaliserT(new ReflectingBasicBlockAnaliser(m_F, m_AAR, m_virtualCallsInfo, m_inputs, m_FAG, B));
    }
    return ReflectingDependencyAnaliserT(
                    new NonDeterministicReflectingBasicBlockAnaliser(m_F, m_AAR, m_virtualCallsInfo, m_inputs, m_FAG, B, depInfo));
}

void LoopAnalysisResult::updateLoopDependecies(llvm::BasicBlock* B)
{
    bool is_latch = m_latches.find(B) != m_latches.end();
    if (m_L.getHeader() == B) {
        auto blockDeps = getBlockTerminatingDependencies(B);
        updateLoopDependecies(std::move(blockDeps));
    } else if (is_latch || m_L.isLoopExiting(B)) {
        DepInfo deps;
        auto pred = pred_begin(B);
        while (pred != pred_end(B)) {
            auto pb = *pred;
            if (isSpecialLoopBlock(pb)) {
                ++pred;
                continue;
            }
            deps.mergeDependencies(getBlockTerminatingDependencies(pb));
            ++pred;
        }
        updateLoopDependecies(std::move(deps));
    }
}

DepInfo LoopAnalysisResult::getBlockTerminatingDependencies(llvm::BasicBlock* B)
{
    const auto& termInstr = B->getTerminator();
    if (termInstr == nullptr) {
        return DepInfo();
    }
    if (auto* branchInstr = llvm::dyn_cast<llvm::BranchInst>(termInstr)) {
        if (branchInstr->isUnconditional()) {
            return DepInfo();
        }
    }
    ValueSet values = Utils::dissolveInstruction(termInstr);
    return DepInfo(DepInfo::VALUE_DEP, values);
}

void LoopAnalysisResult::collectLoopBlocks(llvm::Loop* block_loop)
{
    auto blocks = block_loop->getBlocks();
    auto header = block_loop->getHeader();
    for (auto& block : blocks) {
        m_loopBlocks[block] = header;
    }
}

LoopAnalysisResult::ReflectingDependencyAnaliserT LoopAnalysisResult::createInputDependentAnaliser(llvm::BasicBlock* B)
{
    auto block_loop = m_LI.getLoopFor(B);
    if (block_loop != &m_L) {
        collectLoopBlocks(block_loop);
    }
    return ReflectingDependencyAnaliserT(
                    new ReflectingInputDependentBasicBlockAnaliser(m_F, m_AAR, m_virtualCallsInfo, m_inputs, m_FAG, B));
}

void LoopAnalysisResult::updateLoopDependecies(DepInfo&& depInfo)
{
    m_loopDependencies.mergeDependencies(std::move(depInfo));
}

bool LoopAnalysisResult::checkForLoopDependencies(llvm::BasicBlock* B)
{

    if (!isSpecialLoopBlock(B)) {
        return false;
    }
    if (checkForLoopDependencies(m_BBAnalisers[B]->getValuesDependencies())) {
        return true;
    }
    return checkForLoopDependencies(m_BBAnalisers[B]->getOutParamsDependencies());
}

bool LoopAnalysisResult::checkForLoopDependencies(const DependencyAnaliser::ValueDependencies& valuesDeps)
{
    for (const auto& loopDep : m_loopDependencies.getValueDependencies()) {
        auto pos = valuesDeps.find(loopDep);
        if (pos != valuesDeps.end()) {
            if (pos->second.isInputDep()) {
                return true;
            }
        }
    }
    return false;
}

bool LoopAnalysisResult::checkForLoopDependencies(const DependencyAnaliser::ArgumentDependenciesMap& argDeps)
{
    if (argDeps.empty()) {
        return false;
    }
    for (const auto& loopArgDep : m_loopDependencies.getArgumentDependencies()) {
        auto pos = argDeps.find(loopArgDep);
        if (pos != argDeps.end()) {
            if (pos->second.isInputDep()) {
                return true;
            }
        }
    }
    return false;
}

DepInfo LoopAnalysisResult::getBasicBlockDeps(llvm::BasicBlock* B) const
{
    DepInfo dep(DepInfo::INPUT_INDEP);
    auto pred = pred_begin(B);
    while (pred != pred_end(B)) {
        auto pb = *pred;
        // dependencies of latches, headers and exit blocks are dependencies of whole loop, no need to add them for individual blocks
        if (isSpecialLoopBlock(pb)) {
            ++pred;
            continue;
        }
        const auto& deps = getBlockTerminatingDependencies(pb);
        dep.mergeDependencies(deps);
        ++pred;
    }
    return dep;
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

