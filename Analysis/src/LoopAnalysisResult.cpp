#include "input-dependency/Analysis/LoopAnalysisResult.h"

#include "input-dependency/Analysis/ReflectingBasicBlockAnaliser.h"
#include "input-dependency/Analysis/InputDependentBasicBlockAnaliser.h"
#include "input-dependency/Analysis/NonDeterministicReflectingBasicBlockAnaliser.h"
#include "input-dependency/Analysis/LoopTraversalPath.h"
#include "input-dependency/Analysis/IndirectCallSitesAnalysis.h"
#include "input-dependency/Analysis/Utils.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/PostDominators.h"
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

#include <chrono>

namespace input_dependency {

LoopAnalysisResult::LoopAnalysisResult(llvm::Function* F,
                                       llvm::AAResults& AAR,
                                       const llvm::PostDominatorTree& PDom,
                                       const VirtualCallSiteAnalysisResult& virtualCallsInfo,
                                       const IndirectCallSitesAnalysisResult& indirectCallsInfo,
                                       const Arguments& inputs,
                                       const FunctionAnalysisGetter& Fgetter,
                                       llvm::Loop& L,
                                       llvm::LoopInfo& LI)
                                : m_F(F)
                                , m_AAR(AAR)
                                , m_postDomTree(PDom)
                                , m_virtualCallsInfo(virtualCallsInfo)
                                , m_indirectCallsInfo(indirectCallsInfo)
                                , m_inputs(inputs)
                                , m_FAG(Fgetter)
                                , m_L(L)
                                , m_LI(LI)
                                , m_returnValueDependencies(F->getReturnType())
                                , m_globalsUpdated(false)
                                , m_isReflected(false)
                                , m_is_inputDep(false)
{
    using BlocksVector = llvm::SmallVector<llvm::BasicBlock*, 10>;
    BlocksVector loop_latches;
    m_L.getLoopLatches(loop_latches);
    m_latches.insert(loop_latches.begin(), loop_latches.end());
    loop_latches.clear();
}

DepInfo LoopAnalysisResult::getBlockDependencies() const
{
    return m_loopDependencies;
}

void LoopAnalysisResult::gatherResults()
{
    typedef std::chrono::high_resolution_clock Clock;
    auto tic = Clock::now();

    LoopTraversalPathCreator pathCreator(m_LI, m_L);
    pathCreator.construct();
    auto blocks = pathCreator.getPath();

    //llvm::dbgs() << "Loop will be traversed in order\n";
    //for (const auto& block : blocks) {
    //    llvm::dbgs() << block->getName() << "\n";
    //}

    for (const auto& B : blocks) {
        updateLoopDependecies(B);
        m_BBAnalisers[B] = createDependencyAnaliser(B);
        auto& analiser = m_BBAnalisers[B];
        analiser->setInitialValueDependencies(getBasicBlockPredecessorsDependencies(B));
        analiser->setOutArguments(getBasicBlockPredecessorsArguments(B));
        analiser->setCallbackFunctions(getBasicBlockPredecessorsCallbackFunctions(B));
        analiser->gatherResults();
        updateValueDependencies(B);
        //m_BBAnalisers[B]->dumpResults();
    }
    reflect();
    updateCalledFunctionsList();
    updateReturnValueDependencies();
    updateOutArgumentDependencies();
    updateCallbacks();
    updateValueDependencies();
    reflectValueDepsOnLoopDeps();

    auto toc = Clock::now();
    // only for outer most loops, as it includes analysis of child loops
    if (getenv("LOOP_TIME") && m_L.getLoopDepth() == 1) {
        llvm::dbgs() << "Loop elapsed time " << std::chrono::duration_cast<std::chrono::nanoseconds>(toc - tic).count() << "\n";
    }
}

void LoopAnalysisResult::finalizeResults(const DependencyAnaliser::ArgumentDependenciesMap& dependentArgs)
{
    finalizeLoopDependencies(dependentArgs);
    for (auto& item : m_BBAnalisers) {
        item.second->finalizeResults(dependentArgs);
    }
    m_functionCallInfo.clear();
    updateFunctionCallInfo();
}

void LoopAnalysisResult::finalizeGlobals(const DependencyAnaliser::GlobalVariableDependencyMap& globalsDeps)
{
    finalizeLoopDependencies(globalsDeps);
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

void LoopAnalysisResult::setCallbackFunctions(const std::unordered_map<llvm::Value*, FunctionSet>& callbacks)
{
    m_functionValues = callbacks;
}

void LoopAnalysisResult::setOutArguments(const DependencyAnaliser::ArgumentDependenciesMap& outArgs)
{
    m_outArgDependencies = outArgs;
}

bool LoopAnalysisResult::isInputDependent(llvm::BasicBlock* block) const
{
    if (m_is_inputDep) {
        return true;
    }
    bool is_in_loop = (m_BBAnalisers.find(block) != m_BBAnalisers.end()) || (m_loopBlocks.find(block) != m_loopBlocks.end());
    assert(is_in_loop);
    const auto& analysisRes = getAnalysisResult(block);
    return analysisRes->isInputDependent(block);
}

bool LoopAnalysisResult::isInputDependent(llvm::BasicBlock* block, const DependencyAnaliser::ArgumentDependenciesMap& depArgs) const
{
    if (m_loopDependencies.isInputDep() && m_loopDependencies.getArgumentDependencies().empty()) {
        return true;
    }
    if (!m_loopDependencies.getArgumentDependencies().empty() && Utils::isInputDependentForArguments(m_loopDependencies, depArgs)) {
        return true;
    }
    bool is_in_loop = (m_BBAnalisers.find(block) != m_BBAnalisers.end()) || (m_loopBlocks.find(block) != m_loopBlocks.end());
    assert(is_in_loop);
    const auto& analysisRes = getAnalysisResult(block);
    return analysisRes->isInputDependent(block, depArgs);
}

bool LoopAnalysisResult::isInputDependent(llvm::Instruction* instr) const
{
    auto parentBB = instr->getParent();
    const auto& analysisRes = getAnalysisResult(parentBB);
    return analysisRes->isInputDependent(instr);
}

bool LoopAnalysisResult::isInputDependent(llvm::Instruction* instr,
                                          const DependencyAnaliser::ArgumentDependenciesMap& depArgs) const
{
    auto parentBB = instr->getParent();
    const auto& analysisRes = getAnalysisResult(parentBB);
    return analysisRes->isInputDependent(instr, depArgs);
}

bool LoopAnalysisResult::isInputIndependent(llvm::Instruction* instr) const
{
    auto parentBB = instr->getParent();
    const auto& analysisRes = getAnalysisResult(parentBB);
    return analysisRes->isInputIndependent(instr);
}

bool LoopAnalysisResult::isInputIndependent(llvm::Instruction* instr, const DependencyAnaliser::ArgumentDependenciesMap& depArgs) const
{
    auto parentBB = instr->getParent();
    const auto& analysisRes = getAnalysisResult(parentBB);
    return analysisRes->isInputIndependent(instr, depArgs);
}

bool LoopAnalysisResult::isControlDependent(llvm::Instruction* instr) const
{
    auto parentBB = instr->getParent();
    const auto& analysisRes = getAnalysisResult(parentBB);
    return analysisRes->isControlDependent(instr);
}

bool LoopAnalysisResult::isDataDependent(llvm::Instruction* instr) const
{
    auto parentBB = instr->getParent();
    const auto& analysisRes = getAnalysisResult(parentBB);
    return analysisRes->isDataDependent(instr);
}

bool LoopAnalysisResult::isDataDependent(llvm::Instruction* instr, const ArgumentDependenciesMap& depArgs) const
{
    auto parentBB = instr->getParent();
    const auto& analysisRes = getAnalysisResult(parentBB);
    return analysisRes->isDataDependent(instr, depArgs);
}

bool LoopAnalysisResult::isArgumentDependent(llvm::Instruction* I) const
{
    auto parentBB = I->getParent();
    const auto& analysisRes = getAnalysisResult(parentBB);
    return analysisRes->isArgumentDependent(I);
}

bool LoopAnalysisResult::isArgumentDependent(llvm::BasicBlock* block) const
{
    const auto& analysisRes = getAnalysisResult(block);
    return m_loopDependencies.isInputArgumentDep() || analysisRes->isArgumentDependent(block); 
}

bool LoopAnalysisResult::isGlobalDependent(llvm::Instruction* I) const
{
    auto parentBB = I->getParent();
    const auto& analysisRes = getAnalysisResult(parentBB);
    return analysisRes->isGlobalDependent(I);
}

bool LoopAnalysisResult::hasValueDependencyInfo(llvm::Value* val) const
{
    auto pos = m_valueDependencies.find(val);
    if (pos != m_valueDependencies.end()) {
        return true;
    }
    return m_initialDependencies.find(val) != m_initialDependencies.end();
}

ValueDepInfo LoopAnalysisResult::getValueDependencyInfo(llvm::Value* val)
{
    auto pos = m_valueDependencies.find(val);		
    if (pos != m_valueDependencies.end()) {		
        return pos->second;		
    }		
    auto initial_val_pos = m_initialDependencies.find(val);		
    if (initial_val_pos == m_initialDependencies.end()) {
        return ValueDepInfo();
    }
    // add referenced value		
    DepInfo info = initial_val_pos->second.getValueDep();		
    auto insert_res = m_valueDependencies.insert(std::make_pair(val, ValueDepInfo(val->getType(), info)));		
    return insert_res.first->second;
}

DepInfo LoopAnalysisResult::getInstructionDependencies(llvm::Instruction* instr) const
{
    auto parentBB = instr->getParent();
    auto pos = m_BBAnalisers.find(parentBB);
    if (pos != m_BBAnalisers.end()) {
        return pos->second->getInstructionDependencies(instr);
    }
    if (auto loop = m_LI.getLoopFor(parentBB)) {
        auto parentLoop = Utils::getTopLevelLoop(loop, &m_L);
        parentBB = parentLoop->getHeader();
    } else {
        auto looppos = m_loopBlocks.find(parentBB);
        if (looppos != m_loopBlocks.end()) {
            parentBB = looppos->second;
        }
    }
    pos = m_BBAnalisers.find(parentBB);
    if (pos == m_BBAnalisers.end()) {
        //return DepInfo();
    }
    assert(pos != m_BBAnalisers.end());
    return pos->second->getInstructionDependencies(instr);
}

const DependencyAnaliser::ValueDependencies& LoopAnalysisResult::getValuesDependencies() const
{
    return m_valueDependencies;
}

const DependencyAnaliser::ValueDependencies& LoopAnalysisResult::getInitialValuesDependencies() const
{
    return m_initialDependencies;
}

const ValueDepInfo& LoopAnalysisResult::getReturnValueDependencies() const
{
    return m_returnValueDependencies;
}

const DependencyAnaliser::ArgumentDependenciesMap&
LoopAnalysisResult::getOutParamsDependencies() const
{
    return m_outArgDependencies;
}

const std::unordered_map<llvm::Value*, FunctionSet>&
LoopAnalysisResult::getCallbackFunctions() const
{
    return m_functionValues;
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

bool LoopAnalysisResult::changeFunctionCall(llvm::Instruction* instr, llvm::Function* oldF, llvm::Function* newCallee)
{
    llvm::BasicBlock* parent_block = instr->getParent();
    auto& analysisRes = getAnalysisResult(parent_block);
    const auto called_functions = analysisRes->getCallSitesData();
    if (!analysisRes->changeFunctionCall(instr, oldF, newCallee)) {
        return false;
    }
    assert(analysisRes->hasFunctionCallInfo(newCallee));
    // update called functions
    for (const auto& called_f : called_functions) {
        m_calledFunctions.erase(called_f);
    }
    const auto& new_calls = analysisRes->getCallSitesData();
    m_calledFunctions.insert(new_calls.begin(), new_calls.end());

    // update call site argument deps
    auto callInfo = analysisRes->getFunctionCallInfo(newCallee);
    auto res = m_functionCallInfo.insert(std::make_pair(newCallee, callInfo));
    if (!res.second) {
        res.first->second.addDepInfo(callInfo);
    }
    return true;
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

const LoopAnalysisResult::ReflectingDependencyAnaliserT& LoopAnalysisResult::getAnalysisResult(llvm::BasicBlock* block) const
{
    auto pos = m_BBAnalisers.find(block);
    if (pos != m_BBAnalisers.end()) {
        return pos->second;
    }

    // loop info might be invalidated here. lookup in map
    auto loop_head_pos = m_loopBlocks.find(block);
    assert(loop_head_pos != m_loopBlocks.end());
    auto loop_pos = m_BBAnalisers.find(loop_head_pos->second);
    assert(loop_pos != m_BBAnalisers.end());
    return loop_pos->second;
}

long unsigned LoopAnalysisResult::get_input_dep_blocks_count() const
{
    long unsigned count = 0;
    for (const auto& analysisRes : m_BBAnalisers) {
        count += analysisRes.second->get_input_dep_blocks_count();
    }
    return count;
}

long unsigned LoopAnalysisResult::get_input_indep_blocks_count() const
{
    if (m_is_inputDep) {
        return 0;
    }
    long unsigned count = 0;
    for (const auto& analysisRes : m_BBAnalisers) {
        count += analysisRes.second->get_input_indep_blocks_count();
    }
    return count;
}

long unsigned LoopAnalysisResult::get_input_dep_count() const
{
    long unsigned count = 0;
    for (const auto& analysisRes : m_BBAnalisers) {
        count += analysisRes.second->get_input_dep_count();
    }
    return count;
}

long unsigned LoopAnalysisResult::get_input_indep_count() const
{
    long unsigned count = 0;
    for (const auto& analysisRes : m_BBAnalisers) {
        count += analysisRes.second->get_input_indep_count();
    }
    return count;
}

long unsigned LoopAnalysisResult::get_data_indep_count() const
{
    long unsigned count = 0;
    for (const auto& analysisRes : m_BBAnalisers) {
        count += analysisRes.second->get_data_indep_count();
    }
    return count;
}

long unsigned LoopAnalysisResult::get_input_unknowns_count() const
{
    long unsigned count = 0;
    for (const auto& analysisRes : m_BBAnalisers) {
        count += analysisRes.second->get_input_unknowns_count();
    }
    return count;
}

void LoopAnalysisResult::reflect(const DependencyAnaliser::ValueDependencies& dependencies, const DepInfo& mandatory_deps)
{
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
        DependencyAnaliser::ValueDependencies valueDeps;

        auto pos = m_BBAnalisers.find(*pred);
        if (pos == m_BBAnalisers.end()) {
            auto pred_loop = m_LI.getLoopFor(*pred);
            if (!pred_loop || pred_loop == &m_L) {
                // predecessor is latch
                ++pred;
                continue;
            }
            // predecessor is in another loop (nested loop)
            // in case if pred_loop is not direct child of m_L go up in loop hierarchy until reaches to direct child
            auto pred_pos = m_BBAnalisers.find(pred_loop->getHeader());
            while (pred_loop && pred_pos == m_BBAnalisers.end()) {
                pred_loop = pred_loop->getParentLoop();
                pred_pos = m_BBAnalisers.find(pred_loop->getHeader());
            }
            if (!pred_loop || pred_pos == m_BBAnalisers.end()) {
                continue;
            }
            valueDeps = pred_pos->second->getValuesDependencies();
        } else {
            valueDeps = pos->second->getValuesDependencies();
        }
        for (auto& dep : valueDeps) {
            auto pos = deps.insert(dep);
            if (!pos.second) {
                pos.first->second.getValueDep().mergeDependencies(dep.second.getValueDep());
            }
        }
        ++pred;
    }
    // add initial values. Note values which have been added from prdecessors are not going to be changed
    deps.insert(m_valueDependencies.begin(), m_valueDependencies.end());
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

DependencyAnaliser::ValueCallbackMap LoopAnalysisResult::getBasicBlockPredecessorsCallbackFunctions(llvm::BasicBlock* B)
{
    DependencyAnaliser::ValueCallbackMap callbacks;
    auto pred = pred_begin(B);
    while (pred != pred_end(B)) {
        if (!m_L.contains(*pred)) {
            for (const auto& item : m_functionValues) {
                auto pos = callbacks.insert(item);
                if (!pos.second) {
                    pos.first->second.insert(item.second.begin(), item.second.end());
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
        const auto& functions = pos->second->getCallbackFunctions();
        for (const auto& f : functions) {
            auto pos = callbacks.insert(f);
            if (!pos.second) {
                pos.first->second.insert(f.second.begin(), f.second.end());
            }
        }
        ++pred;
    }
    return callbacks;
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
    // latches should contain all the info
    for (const auto& latch : m_latches) {
        const auto& outArgs = m_BBAnalisers[latch]->getOutParamsDependencies();
        for (const auto& item : outArgs) {
            auto pos = m_outArgDependencies.find(item.first);
            assert(pos != m_outArgDependencies.end());
            pos->second.mergeDependencies(item.second);
        }
    }
}

void LoopAnalysisResult::updateCallbacks()
{
    // latches should contain all the info
    for (const auto& latch : m_latches) {
        const auto& callbacks = m_BBAnalisers[latch]->getCallbackFunctions();
        for (const auto& item : callbacks) {
            auto res = m_functionValues.insert(item);
            if (!res.second) {
                res.first->second.clear();
                res.first->second = item.second;
            }
        }
    }
}

void LoopAnalysisResult::updateValueDependencies()
{
    m_valueDependencies.clear();
    for (const auto& BB_analiser : m_BBAnalisers) {
        const auto& valuesDeps = BB_analiser.second->getValuesDependencies();
        for (const auto& item : valuesDeps) {
            auto pos = m_valueDependencies.insert(item);
            if (!pos.second) {
                pos.first->second.mergeDependencies(item.second);
            }
        }
    }
}

void LoopAnalysisResult::updateValueDependencies(llvm::BasicBlock* B)
{
    const auto& block_deps = m_BBAnalisers[B]->getValuesDependencies();
    for (const auto& val : block_deps) {
        auto pos = m_valueDependencies.insert(val);
        if (!pos.second) {
            pos.first->second = val.second;
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
        auto pos = m_BBAnalisers.find(latch);
        if (pos == m_BBAnalisers.end()) {
            auto latch_loop = m_LI.getLoopFor(latch);
            if (latch_loop == &m_L) {
                llvm::dbgs() << "Can't find loop for latch " << latch->getName() << "\n";
            }
        }
        if (pos == m_BBAnalisers.end()) {
            llvm::dbgs() << "blah " << latch->getName() << "\n";
        }
        assert(pos != m_BBAnalisers.end());
        auto valueDeps = pos->second->getValuesDependencies();
        for (const auto& dep : valueDeps) {
            auto res = valueDependencies.insert(dep);
            if (!res.second) {
                res.first->second.getValueDep().mergeDependencies(dep.second.getValueDep());
            }
        }
        auto initialValueDeps = pos->second->getInitialValuesDependencies();
        for (const auto& dep : initialValueDeps) {
            auto res = valueDependencies.insert(dep);
            if (!res.second) {
                res.first->second.getValueDep().mergeDependencies(dep.second.getValueDep());
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
        LoopAnalysisResult* loopAnalysisResult = new LoopAnalysisResult(m_F, m_AAR, m_postDomTree,
                                                                        m_virtualCallsInfo,
                                                                        m_indirectCallsInfo,
                                                                        m_inputs, m_FAG, *block_loop, m_LI);
        loopAnalysisResult->setLoopDependencies(depInfo);
        collectLoopBlocks(block_loop);
        return ReflectingDependencyAnaliserT(loopAnalysisResult);
    }
    // loop argument dependencies will also become basic blocks argument dependencies.
    // this should not make runtime worse as argument dependencies does not affect reflection algorithm. 
    //depInfo.mergeDependencies(m_loopDependencies.getArgumentDependencies());
    depInfo.mergeDependencies(m_loopDependencies);
    if (depInfo.isInputArgumentDep()) {
        depInfo.mergeDependency(DepInfo::INPUT_ARGDEP);
    }
    if (depInfo.isInputIndep()) {
        return ReflectingDependencyAnaliserT(new ReflectingBasicBlockAnaliser(m_F, m_AAR,
                                                                              m_virtualCallsInfo,
                                                                              m_indirectCallsInfo,
                                                                              m_inputs, m_FAG, B));
    }
    return ReflectingDependencyAnaliserT(
                    new NonDeterministicReflectingBasicBlockAnaliser(m_F, m_AAR, m_virtualCallsInfo, m_indirectCallsInfo,
                                                                     m_inputs, m_FAG, B, depInfo));
}

void LoopAnalysisResult::updateLoopDependecies(llvm::BasicBlock* B)
{
    bool is_latch = m_latches.find(B) != m_latches.end();
    if (m_L.getHeader() == B) {
        auto blockDeps = getBlockTerminatingDependencies(B);
        if (blockDeps.isDefined()) {
            updateLoopDependecies(std::move(blockDeps));
        }
    } else if (is_latch || m_L.isLoopExiting(B)) {
        DepInfo deps = getBlockTerminatingDependencies(B);
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

void LoopAnalysisResult::finalizeLoopDependencies(const DependencyAnaliser::GlobalVariableDependencyMap& globalsDeps)
{
    if (!m_isReflected) {
        reflectValueDepsOnLoopDeps();
        m_isReflected = true;
    }
    if (!m_loopDependencies.isValueDep()) {
        return;
    }
    if (m_loopDependencies.isInputDep()) {
        m_is_inputDep = true;
        m_loopDependencies.mergeDependencies(DepInfo::INPUT_DEP);
        return;
    }
    auto loopValueDependencies = m_loopDependencies.getValueDependencies();
    for (const auto& dep : loopValueDependencies) {
        auto* global = llvm::dyn_cast<llvm::GlobalVariable>(dep);
        if (!global) {
            continue;
        }
        auto global_dep = globalsDeps.find(global);
        if (global_dep == globalsDeps.end()) {
            continue;
        }
        m_loopDependencies.mergeDependencies(global_dep->second.getValueDep());
        m_loopDependencies.getValueDependencies().erase(dep);
    }
}

void LoopAnalysisResult::finalizeLoopDependencies(const DependencyAnaliser::ArgumentDependenciesMap& dependentArgs)
{
    if (!m_isReflected) {
        reflectValueDepsOnLoopDeps();
        m_isReflected = true;
    }
    if (m_loopDependencies.isValueDep()) {
        if (!m_loopDependencies.isInputArgumentDep()) {
            m_loopDependencies.setDependency(DepInfo::INPUT_INDEP);
        }
    }
    if (m_loopDependencies.isInputDep()) {
        m_is_inputDep = true;
        m_loopDependencies.mergeDependencies(DepInfo::INPUT_DEP);
    } else if (m_loopDependencies.isInputArgumentDep()
            && Utils::haveIntersection(dependentArgs, m_loopDependencies.getArgumentDependencies())) {
        m_is_inputDep = true;
        m_loopDependencies.mergeDependencies(DepInfo::INPUT_DEP);
    }
}

void LoopAnalysisResult::reflectValueDepsOnLoopDeps()
{
    auto loop_dependencies = m_loopDependencies.getValueDependencies();
    ValueSet erase_values;
    if (!loop_dependencies.empty()) {
        for (auto& loopDep : loop_dependencies) {
            auto dep = m_valueDependencies.find(loopDep);
            if (dep != m_valueDependencies.end()) {
                m_loopDependencies.mergeDependencies(dep->second.getValueDep());
                erase_values.insert(dep->first);
            }
        }
    }
    for (auto val : erase_values) {
        m_loopDependencies.getValueDependencies().erase(val);
    }
}

LoopAnalysisResult::ReflectingDependencyAnaliserT LoopAnalysisResult::createInputDependentAnaliser(llvm::BasicBlock* B)
{
    auto block_loop = m_LI.getLoopFor(B);
    if (block_loop != &m_L) {
        LoopAnalysisResult* loopAnalysisResult = new LoopAnalysisResult(m_F, m_AAR, m_postDomTree,
                                                                        m_virtualCallsInfo,
                                                                        m_indirectCallsInfo,
                                                                        m_inputs, m_FAG, *block_loop, m_LI);
        loopAnalysisResult->setLoopDependencies(DepInfo(DepInfo::INPUT_DEP));
        collectLoopBlocks(block_loop);
        return ReflectingDependencyAnaliserT(loopAnalysisResult);
    }
    return ReflectingDependencyAnaliserT(
                    new ReflectingInputDependentBasicBlockAnaliser(m_F, m_AAR, m_virtualCallsInfo, m_indirectCallsInfo, m_inputs, m_FAG, B));
}

void LoopAnalysisResult::updateLoopDependecies(DepInfo&& depInfo)
{
    m_loopDependencies.mergeDependencies(std::move(depInfo));
}

DepInfo LoopAnalysisResult::getBasicBlockDeps(llvm::BasicBlock* B) const
{
    DepInfo dep(DepInfo::INPUT_INDEP);
    bool postdominates_all_predecessors = true;
    const auto& b_node = m_postDomTree[B];
    auto pred = pred_begin(B);
    while (pred != pred_end(B)) {
        auto pb = *pred;
        // dependencies of latches, headers and exit blocks are dependencies of whole loop, no need to add them for individual blocks
        if (isSpecialLoopBlock(pb)) {
            ++pred;
            continue;
        }
        // predecessor is in another loop. block B is the only block which can have predecessor in other(nested) loop.
        // As all loops are considered to be exiting (no infinite loops), B will be executed indipendent on nested loop.
        if (m_LI.getLoopFor(pb) != &m_L) {
            ++pred;
            continue;
        }
        const auto& deps = getBlockTerminatingDependencies(pb);
        dep.mergeDependencies(deps);
        auto pred_node = m_postDomTree[*pred];
        postdominates_all_predecessors &= m_postDomTree.dominates(b_node, pred_node);
        ++pred;
    }
    // for a normal loops this will never be true, as there always is another path from loop header to exit block
    //llvm::BasicBlock* header = m_L.getHeader();
    //auto header_node = m_postDomTree[header];
    //postdominates_all_predecessors &= m_postDomTree.dominates(b_node, header_node);
    if (postdominates_all_predecessors) {
        return DepInfo(DepInfo::INPUT_INDEP);
    }
    return dep;
}

DepInfo LoopAnalysisResult::getBlockTerminatingDependencies(llvm::BasicBlock* B) const
{
    const auto& termInstr = B->getTerminator();
    if (termInstr == nullptr) {
        return DepInfo(DepInfo::INPUT_DEP);
    }
    //if (auto* branchInstr = llvm::dyn_cast<llvm::BranchInst>(termInstr)) {
    //    if (branchInstr->isUnconditional()) {
    //        return DepInfo();
    //    }
    //}
    auto pos = m_BBAnalisers.find(B);
    if (pos == m_BBAnalisers.end()) {
        ValueSet values = Utils::dissolveInstruction(termInstr);
        if (values.empty()) {
            return DepInfo(DepInfo::INPUT_INDEP);
        }
        return DepInfo(DepInfo::VALUE_DEP, values);
    }
    return pos->second->getInstructionDependencies(termInstr);
}

} // namespace input_dependency

