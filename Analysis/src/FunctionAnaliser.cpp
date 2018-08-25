#include "input-dependency/Analysis/FunctionAnaliser.h"

#include "input-dependency/Analysis/BasicBlockAnalysisResult.h"
#include "input-dependency/Analysis/DependencyAnalysisResult.h"
#include "input-dependency/Analysis/DependencyAnaliser.h"
#include "input-dependency/Analysis/LoopAnalysisResult.h"
#include "input-dependency/Analysis/NonDeterministicBasicBlockAnaliser.h"
#include "input-dependency/Analysis/IndirectCallSitesAnalysis.h"
#include "input-dependency/Analysis/BasicBlocksUtils.h"
#include "input-dependency/Analysis/Utils.h"
#include "input-dependency/Analysis/ClonedFunctionAnalysisResult.h"
#include "input-dependency/Analysis/CFGTraversalPath.h"
#include "input-dependency/Analysis/InputDepConfig.h"
#include "input-dependency/Analysis/exception.h"

#include "llvm/ADT/SCCIterator.h"
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
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

#include <chrono>
#include <forward_list>
#include <list>

namespace input_dependency {

namespace {

llvm::Value* get_mapped_value(llvm::Value* val, llvm::ValueToValueMapTy& VMap)
{
    auto map_pos = VMap.find(val);
    if (map_pos == VMap.end()) {
        llvm::dbgs() << "No mapping for Value " << *val << ". Skip.\n";
        return nullptr;
    }
    return map_pos->second;
}

llvm::Instruction* get_mapped_instruction(llvm::Instruction* I,
                                          std::unordered_map<llvm::Instruction*, llvm::Instruction*>& instr_map,
                                          llvm::ValueToValueMapTy& VMap)
{
    auto local_map_pos = instr_map.find(I);
    if (local_map_pos != instr_map.end()) {
        return local_map_pos->second;
    }
    llvm::Value* mapped_value = get_mapped_value(I, VMap);
    if (!mapped_value) {
        return nullptr;
    }
    llvm::Instruction* mapped_instr = llvm::dyn_cast<llvm::Instruction>(mapped_value);
    if (!mapped_instr) {
        llvm::dbgs() << "Invalid mapping for instruction " << *I << ". Mapped to value " << *mapped_value << ". Skip.\n";
        return nullptr;
    }
    instr_map[I] = mapped_instr;
    return mapped_instr;
}

llvm::Argument* get_mapped_argument(llvm::Argument* arg,
                                    std::unordered_map<llvm::Argument*, llvm::Argument*>& argument_mapping,
                                    llvm::ValueToValueMapTy& VMap)
{
    auto local_map_pos = argument_mapping.find(arg);
    if (local_map_pos != argument_mapping.end()) {
        return local_map_pos->second;
    }
    llvm::Value* mapped_value = get_mapped_value(arg, VMap);
    if (!mapped_value) {
        return nullptr;
    }
    llvm::Argument* mapped_arg = llvm::dyn_cast<llvm::Argument>(mapped_value);
    if (!arg) {
        llvm::dbgs() << "Invalid mapping for argument " << *arg
            << ". Mapped to value " << *mapped_value << ". Skip.\n";
        return nullptr;
    }
    argument_mapping[arg] = mapped_arg;
    return mapped_arg;
}

}

class FunctionAnaliser::Impl
{
public:
    Impl(llvm::Function* F,
         const FunctionAnalysisGetter& getter)
        : m_F(F)
        , m_FAGetter(getter)
        , m_returnValueDependencies(F->getReturnType())
        , m_argumentsFinalized(false)
        , m_globalsFinalized(false)
        , m_globalsUpdated(false)
        , m_is_inputDep(false)
        , m_is_extracted(false)
    {
    }

    void setFunction(llvm::Function* F)
    {
        m_F = F;
    }

    void setAAResults(llvm::AAResults* AAR)
    {
        m_AAR = AAR;
    }

    void setLoopInfo(llvm::LoopInfo* LI)
    {
        m_LI = LI;
    }

    void setPostDomTree(const llvm::PostDominatorTree* PDom)
    {
        m_postDomTree = PDom;
    }

    void setDomTree(const llvm::DominatorTree* dom)
    {
        m_domTree = dom;
    }

    void setVirtualCallSiteAnalysisResult(const VirtualCallSiteAnalysisResult* virtualCallsInfo)
    {
        m_virtualCallsInfo = virtualCallsInfo;
    }

    void setIndirectCallSiteAnalysisResult(const IndirectCallSitesAnalysisResult* indirectCallsInfo)
    {
        m_indirectCallsInfo = indirectCallsInfo;
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

    bool isInputDepFunction() const
    {
        return m_is_inputDep;
    }

    void setIsInputDepFunction(bool isInputDep)
    {
        m_is_inputDep = isInputDep;
        updateFunctionInputDependencies();
    }

    bool isExtractedFunction() const
    {
        return m_is_extracted;
    }

    void setIsExtractedFunction(bool isExtracted)
    {
        m_is_extracted = isExtracted;
    }

    bool areArgumentsFinalized() const
    {
        return m_argumentsFinalized;
    }

    bool areGlobalsFinalized() const
    {
        return m_globalsFinalized;
    }

private:
    using ArgumentDependenciesMap = DependencyAnaliser::ArgumentDependenciesMap;
    using GlobalVariableDependencyMap = DependencyAnaliser::GlobalVariableDependencyMap;
    using DependencyAnalysisResultT = std::shared_ptr<DependencyAnalysisResult>;
    using FunctionArgumentsDependencies = std::unordered_map<llvm::Function*, ArgumentDependenciesMap>;
    using FunctionGlobalsDependencies = std::unordered_map<llvm::Function*, GlobalVariableDependencyMap>;

public:
    bool isInputDependent(llvm::Instruction* instr) const;
    bool isInputIndependent(llvm::Instruction* instr) const;
    bool isInputDependent(llvm::Value* value) const;
    bool isInputIndependent(llvm::Value* value) const;
    bool isInputDependentBlock(llvm::BasicBlock* block) const;
    bool isControlDependent(llvm::Instruction* I) const;
    bool isDataDependent(llvm::Instruction* I) const;
    bool isArgumentDependent(llvm::Instruction* I) const;
    bool isArgumentDependent(llvm::BasicBlock* block) const;
    bool isGlobalDependent(llvm::Instruction* I) const;
    bool isOutArgInputIndependent(llvm::Argument* arg) const;
    ValueDepInfo getOutArgDependencies(llvm::Argument* arg) const;
    bool isReturnValueInputIndependent() const;
    const ValueDepInfo& getRetValueDependencies() const;
    bool hasGlobalVariableDepInfo(llvm::GlobalVariable* global) const;
    ValueDepInfo getGlobalVariableDependencies(llvm::GlobalVariable* global) const;
    ValueDepInfo getDependencyInfoFromBlock(llvm::Value* val, llvm::BasicBlock* block) const;
    DepInfo getBlockDependencyInfo(llvm::BasicBlock* block) const;
    // Returns collected data for function calls in this function
    const DependencyAnaliser::ArgumentDependenciesMap& getCallArgumentInfo(llvm::Function* F) const;
    FunctionCallDepInfo getFunctionCallDepInfo(llvm::Function* F) const;
    bool changeFunctionCall(llvm::Instruction* callInstr, llvm::Function* oldF, llvm::Function* newF);
    const DependencyAnaliser::GlobalVariableDependencyMap& getCallGlobalsInfo(llvm::Function* F) const;
    const GlobalsSet& getReferencedGlobals() const;
    const GlobalsSet& getModifiedGlobals() const;

    void analyze();
    void finalizeArguments(const ArgumentDependenciesMap& dependentArgNos);
    void finalizeGlobals(const GlobalVariableDependencyMap& globalsDeps);
    long unsigned get_input_dep_blocks_count() const;
    long unsigned get_input_indep_blocks_count() const;
    long unsigned get_unreachable_blocks_count() const;
    long unsigned get_unreachable_instructions_count() const;
    long unsigned get_input_dep_count() const;
    long unsigned get_input_indep_count() const;
    long unsigned get_data_indep_count() const;
    long unsigned get_input_unknowns_count() const;
    FunctionInputDependencyResultInterface* cloneForArguments(const DependencyAnaliser::ArgumentDependenciesMap& inputDepArgs);
    void dump() const;

private:
    using BlocksInTraversalOrder = std::list<std::pair<llvm::BasicBlock*, llvm::Loop*>>;
    void collectArguments();
    DependencyAnalysisResultT createBasicBlockAnalysisResult(llvm::BasicBlock* B,
                                                             const DepInfo& depInfo);
    LoopAnalysisResult* createLoopAnalysisResult(const DepInfo& depInfo, llvm::Loop* loop);
    DepInfo getBasicBlockPredecessorInstructionsDeps(llvm::BasicBlock* B) const;

    void updateFunctionInputDependencies();
    void updateFunctionCallInfo(llvm::Function* F);
    void updateFunctionCallsInfo(llvm::BasicBlock* B);
    void updateFunctionCallInfo(llvm::BasicBlock* B, llvm::Function* F);
    void updateFunctionCallGlobalsInfo(llvm::Function* F);
    void updateFunctionCallsGlobalsInfo(llvm::BasicBlock* B);
    void updateFunctionCallGlobalsInfo(llvm::BasicBlock* B, llvm::Function* F);

    void updateValueDependencies(llvm::BasicBlock* B);
    void updateCalledFunctionsList(const DependencyAnalysisResultT& BAR);
    void updateReturnValueDependencies(llvm::BasicBlock* B);
    void updateOutArgumentDependencies(llvm::BasicBlock* B);
    void updateGlobals();
    void updateReferencedGlobals();
    void updateModifiedGlobals();
    DependencyAnaliser::ValueDependencies getBasicBlockPredecessorsDependencies(llvm::BasicBlock* B);
    DependencyAnaliser::ArgumentDependenciesMap getBasicBlockPredecessorsArguments(llvm::BasicBlock* B);
    DependencyAnaliser::ValueCallbackMap getBasicBlockPredecessorsCallbackFunctions(llvm::BasicBlock* B);
    DependencyAnalysisResultT getAnalysisResult(llvm::BasicBlock* B) const;

private:
    llvm::Function* m_F;
    llvm::AAResults* m_AAR;
    llvm::LoopInfo* m_LI;
    const llvm::PostDominatorTree* m_postDomTree;
    const llvm::DominatorTree* m_domTree;
    const VirtualCallSiteAnalysisResult* m_virtualCallsInfo;
    const IndirectCallSitesAnalysisResult* m_indirectCallsInfo;
    const FunctionAnalysisGetter& m_FAGetter;

    Arguments m_inputs;
    DependencyAnaliser::ValueDependencies m_valueDependencies; // all value dependencies
    DependencyAnaliser::ArgumentDependenciesMap m_outArgDependencies;
    ValueDepInfo m_returnValueDependencies;
    FunctionArgumentsDependencies m_calledFunctionsInfo;
    FunctionGlobalsDependencies m_calledFunctionGlobalsInfo;
    FunctionSet m_calledFunctions;
    GlobalsSet m_referencedGlobals;
    GlobalsSet m_modifiedGlobals;
    bool m_argumentsFinalized;
    bool m_globalsFinalized;
    bool m_globalsUpdated;
    bool m_is_inputDep;
    bool m_is_extracted;

    std::unordered_map<llvm::BasicBlock*, DependencyAnalysisResultT> m_BBAnalysisResults;
    // LoopInfo will be invalidated after analisis, instead of keeping copy of it, keep this map.
    std::unordered_map<llvm::BasicBlock*, llvm::BasicBlock*> m_loopBlocks;
    // last block of a function is not always the exit block, as it may be unreachable from entry
    llvm::BasicBlock* m_exit_block;
}; // class FunctionAnaliser::Impl


bool FunctionAnaliser::Impl::isInputDependent(llvm::Instruction* instr) const
{
    const auto& analysisRes = getAnalysisResult(instr->getParent());
    if (analysisRes) {
        return analysisRes->isInputDependent(instr);
    }
    return true;
}

bool FunctionAnaliser::Impl::isInputIndependent(llvm::Instruction* instr) const
{
    const auto& analysisRes = getAnalysisResult(instr->getParent());
    if (analysisRes) {
        return analysisRes->isInputIndependent(instr);
    }
    return false;
}

bool FunctionAnaliser::Impl::isInputIndependent(llvm::Value* val) const
{
    auto pos = m_valueDependencies.find(val);
    if (pos == m_valueDependencies.end()) {
        return false; // ??
    }
    return pos->second.isInputIndep();
}

bool FunctionAnaliser::Impl::isInputDependentBlock(llvm::BasicBlock* block) const
{
    const auto& analysisRes = getAnalysisResult(block);
    if (analysisRes) {
        return analysisRes->isInputDependent(block);
    }
    return true;
}

bool FunctionAnaliser::Impl::isControlDependent(llvm::Instruction* I) const
{
    return m_is_inputDep || isInputDependentBlock(I->getParent());
}

bool FunctionAnaliser::Impl::isDataDependent(llvm::Instruction* I) const
{
    if (isInputIndependent(I)) {
        return false;
    }
    const auto& analysisRes = getAnalysisResult(I->getParent());
    if (analysisRes) {
        return analysisRes->isDataDependent(I);
    }
    return true;
}

bool FunctionAnaliser::Impl::isArgumentDependent(llvm::Instruction* I) const
{
    const auto& analysisRes = getAnalysisResult(I->getParent());
    if (analysisRes) {
        return analysisRes->isArgumentDependent(I);
    }
    return false;
}

bool FunctionAnaliser::Impl::isArgumentDependent(llvm::BasicBlock* block) const
{
    const auto& analysisRes = getAnalysisResult(block);
    if (analysisRes) {
        return analysisRes->isArgumentDependent(block);
    }
    return false;
}

bool FunctionAnaliser::Impl::isGlobalDependent(llvm::Instruction* I) const
{
    const auto& analysisRes = getAnalysisResult(I->getParent());
    if (analysisRes) {
        return analysisRes->isGlobalDependent(I);
    }
    return false;
}

bool FunctionAnaliser::Impl::isOutArgInputIndependent(llvm::Argument* arg) const
{
    auto pos = m_outArgDependencies.find(arg);
    if (pos == m_outArgDependencies.end()) {
        return true;
    }
    return pos->second.isInputIndep();
}

ValueDepInfo FunctionAnaliser::Impl::getOutArgDependencies(llvm::Argument* arg) const
{
    auto pos = m_outArgDependencies.find(arg);
    if (pos == m_outArgDependencies.end()) {
        return ValueDepInfo();
    }
    return pos->second;
}

bool FunctionAnaliser::Impl::isReturnValueInputIndependent() const
{
    return m_returnValueDependencies.isInputIndep();
}

const ValueDepInfo& FunctionAnaliser::Impl::getRetValueDependencies() const
{
    return m_returnValueDependencies;
}
 
bool FunctionAnaliser::Impl::hasGlobalVariableDepInfo(llvm::GlobalVariable* global) const
{
    const auto& pos = m_BBAnalysisResults.find(m_exit_block);
    assert(pos != m_BBAnalysisResults.end());
    llvm::Value* val = llvm::dyn_cast<llvm::GlobalVariable>(global);
    assert(val != nullptr);
    return pos->second->hasValueDependencyInfo(val);
}

ValueDepInfo FunctionAnaliser::Impl::getGlobalVariableDependencies(llvm::GlobalVariable* global) const
{
    const auto& pos = m_BBAnalysisResults.find(m_exit_block);
    assert(pos != m_BBAnalysisResults.end());
    llvm::Value* val = llvm::dyn_cast<llvm::GlobalVariable>(global);
    assert(val != nullptr);
    return pos->second->getValueDependencyInfo(val);
}

ValueDepInfo FunctionAnaliser::Impl::getDependencyInfoFromBlock(llvm::Value* val, llvm::BasicBlock* block) const
{
    if (val == nullptr || block == nullptr) {
        return ValueDepInfo();
    }
    if (auto global = llvm::dyn_cast<llvm::GlobalVariable>(val)) {
        return getGlobalVariableDependencies(global);
    }
    const auto& analysisRes = getAnalysisResult(block);
    if (!analysisRes) {
        return ValueDepInfo();
    }
    if (analysisRes->hasValueDependencyInfo(val)) {
        return analysisRes->getValueDependencyInfo(val);
    }
    auto instr = llvm::dyn_cast<llvm::Instruction>(val);
    assert(instr != nullptr);
    if (instr->getParent() == block) {
        return ValueDepInfo(val->getType(), analysisRes->getInstructionDependencies(instr));
    }
    return ValueDepInfo();
}

DepInfo FunctionAnaliser::Impl::getBlockDependencyInfo(llvm::BasicBlock* block) const
{
    const auto& analysisRes = getAnalysisResult(block);
    if (!analysisRes) {
        return DepInfo();
    }
    return analysisRes->getBlockDependencies();
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

bool FunctionAnaliser::Impl::changeFunctionCall(llvm::Instruction* callInstr, llvm::Function* oldF, llvm::Function* newF)
{
    llvm::BasicBlock* block = callInstr->getParent();
    auto analysisRes = getAnalysisResult(block);
    if (!analysisRes) {
        //llvm::dbgs() << "Did not find parent block of call " << *callInstr << "\n";
        return false;
    }
    const auto called_functions = analysisRes->getCallSitesData();
    bool res = analysisRes->changeFunctionCall(callInstr, oldF, newF);
    if (res) {
        for (const auto& called_f : called_functions) {
            m_calledFunctions.erase(called_f);
        }
        updateCalledFunctionsList(analysisRes);
    }
    return res;
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

void FunctionAnaliser::Impl::analyze()
{
    typedef std::chrono::high_resolution_clock Clock;
    auto tic = Clock::now();
    collectArguments();

    CFGTraversalPathCreator traversalPath(*m_F);
    traversalPath.setLoopInfo(m_LI);
    traversalPath.setDomTree(m_domTree);
    traversalPath.construct(CFGTraversalPathCreator::CFG);
    const auto& blocks_in_traversal_order = traversalPath.getBlocksInOrder();
    m_loopBlocks= traversalPath.getBlocksLoops();
    llvm::BasicBlock* bb;
    for (auto& block : blocks_in_traversal_order) {
        bb = block.first;
        //llvm::dbgs() << "process block: " << bb->getName() << "\n";
        const auto& depInfo = getBasicBlockPredecessorInstructionsDeps(bb);
        if (block.second) {
            m_BBAnalysisResults[bb].reset(createLoopAnalysisResult(depInfo, block.second));
        } else {
            m_BBAnalysisResults[bb] = createBasicBlockAnalysisResult(bb, depInfo);
        }
        m_BBAnalysisResults[bb]->setInitialValueDependencies(getBasicBlockPredecessorsDependencies(bb));
        m_BBAnalysisResults[bb]->setOutArguments(getBasicBlockPredecessorsArguments(bb));
        m_BBAnalysisResults[bb]->setCallbackFunctions(getBasicBlockPredecessorsCallbackFunctions(bb));
        m_BBAnalysisResults[bb]->gatherResults();

        updateValueDependencies(bb);
        updateCalledFunctionsList(m_BBAnalysisResults[bb]);
        updateReturnValueDependencies(bb);
        updateOutArgumentDependencies(bb);
    }
    m_exit_block = bb;
    m_inputs.clear();
    auto toc = Clock::now();
    if (getenv("INPUT_DEP_TIME")) {
        llvm::dbgs() << "Input dep elapsed time " << std::chrono::duration_cast<std::chrono::nanoseconds>(toc - tic).count() << "\n";
    }
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
    updateFunctionInputDependencies();
    m_argumentsFinalized = true;
}

void FunctionAnaliser::Impl::finalizeGlobals(const GlobalVariableDependencyMap& globalsDeps)
{
    for (auto& item : m_BBAnalysisResults) {
        item.second->finalizeGlobals(globalsDeps);
    }
    updateFunctionInputDependencies();
    m_globalsFinalized = true;
}

long unsigned FunctionAnaliser::Impl::get_input_dep_blocks_count() const
{
    long unsigned count = 0;
    for (const auto& analiser : m_BBAnalysisResults) {
        count += analiser.second->get_input_dep_blocks_count();
    }
    return count;
}

long unsigned FunctionAnaliser::Impl::get_input_indep_blocks_count() const
{
    long unsigned count = 0;
    for (const auto& analiser : m_BBAnalysisResults) {
        count += analiser.second->get_input_indep_blocks_count();
    }
    return count;
}

long unsigned FunctionAnaliser::Impl::get_unreachable_blocks_count() const
{
    return BasicBlocksUtils::get().getFunctionUnreachableBlocksCount(m_F);
}

long unsigned FunctionAnaliser::Impl::get_unreachable_instructions_count() const
{
    return BasicBlocksUtils::get().getFunctionUnreachableInstructionsCount(m_F);
}

long unsigned FunctionAnaliser::Impl::get_input_dep_count() const
{
    long unsigned count = 0;
    for (const auto& analiser : m_BBAnalysisResults) {
        count += analiser.second->get_input_dep_count();
    }
    return count;
}

long unsigned FunctionAnaliser::Impl::get_input_indep_count() const
{
    long unsigned count = 0;
    for (const auto& analiser : m_BBAnalysisResults) {
        count += analiser.second->get_input_indep_count();
    }
    return count;
}

long unsigned FunctionAnaliser::Impl::get_data_indep_count() const
{
    long unsigned count = 0;
    for (const auto& analiser : m_BBAnalysisResults) {
        count += analiser.second->get_data_indep_count();
    }
    return count;
}

long unsigned FunctionAnaliser::Impl::get_input_unknowns_count() const
{
    long unsigned count = 0;
    for (const auto& analiser : m_BBAnalysisResults) {
        count += analiser.second->get_input_unknowns_count();
    }
    return count;
}

FunctionInputDependencyResultInterface*
FunctionAnaliser::Impl::cloneForArguments(const DependencyAnaliser::ArgumentDependenciesMap& inputDepArgs)
{
    llvm::ValueToValueMapTy VMap;
    llvm::Function* newF = llvm::CloneFunction(m_F, VMap);

    ClonedFunctionAnalysisResult* clonedResults = new ClonedFunctionAnalysisResult(newF);
    clonedResults->setCalledFunctions(m_calledFunctions);

    // get clonned finalized info
    InstrSet inputDeps;
    InstrSet inputIndeps;
    InstrSet dataDeps;
    InstrSet globalDeps;
    std::unordered_set<llvm::BasicBlock*> inputDepBlocks;
    std::unordered_map<llvm::Instruction*, llvm::Instruction*> local_instr_map;
    for (auto& B : *m_F) {
        auto analysisRes = getAnalysisResult(&B);
        bool isInputDepBlock = false;
        // if analysisRes is null, consider input dependent
        if (!analysisRes || analysisRes->isInputDependent(&B, inputDepArgs)) {
            llvm::Value* block_val = get_mapped_value(&B, VMap);
            if (!block_val) {
                continue;
            }
            llvm::BasicBlock* mapped_block = llvm::dyn_cast<llvm::BasicBlock>(block_val);
            if (!mapped_block) {
                llvm::dbgs() << "Invalid mapping for block " << B.getName()
                             << ". Mapped to value " << *block_val << ". Skip entire block.\n";
                continue;
            }
            inputDepBlocks.insert(mapped_block);
            isInputDepBlock = true;
            if (BasicBlocksUtils::get().isBlockUnreachable(&B)) {
                BasicBlocksUtils::get().addUnreachableBlock(mapped_block);
            }
        }
        for (auto& I : B) {
            llvm::Instruction* mapped_instr = get_mapped_instruction(&I, local_instr_map, VMap);
            if (!mapped_instr) {
                continue;
            }
            if (analysisRes->isGlobalDependent(&I)) {
                globalDeps.insert(mapped_instr);
            }
            if (!analysisRes || analysisRes->isInputDependent(&I, inputDepArgs)) {
                inputDeps.insert(mapped_instr);
                if (analysisRes->isDataDependent(&I, inputDepArgs)) {
                    dataDeps.insert(mapped_instr);
                }
            } else if (analysisRes && analysisRes->isInputIndependent(&I, inputDepArgs)) {
                inputIndeps.insert(mapped_instr);
            } else {
                llvm::dbgs() << "No information for instruction " << I << "\n";
            }
        }
    }
    clonedResults->setInputDependentBasicBlocks(std::move(inputDepBlocks));
    clonedResults->setInputDepInstrs(std::move(inputDeps));
    clonedResults->setDataDependentInstrs(std::move(dataDeps));
    clonedResults->setInputIndepInstrs(std::move(inputIndeps));
    clonedResults->setGlobalDependentInstrs(std::move(globalDeps));

    // clone call site information
    std::unordered_map<llvm::Function*, FunctionCallDepInfo> clonned_call_dep_info;
    for (auto& F : m_calledFunctions) {
        auto callDepInfo = getFunctionCallDepInfo(F);
        FunctionCallDepInfo cloned_depInfo(*F);
        const auto& callSiteArgDeps = callDepInfo.getCallsArgumentDependencies();
        for (const auto& callsite_entry : callSiteArgDeps) {
            // first - call instruction
            // second - arg deps
            llvm::Instruction* mapped_instr = get_mapped_instruction(const_cast<llvm::Instruction*>(callsite_entry.first),
                                                                     local_instr_map, VMap);
            if (!mapped_instr) {
                continue;
            }
            ArgumentDependenciesMap clonedArgDeps;
            std::unordered_map<llvm::Argument*, llvm::Argument*> argument_mapping;
            for (auto& argdep_entry : callsite_entry.second) {
                ValueDepInfo depInfo = argdep_entry.second;
                if (Utils::isInputDependentForArguments(argdep_entry.second.getValueDep(), inputDepArgs)) {
                    depInfo.setDependency(DepInfo::INPUT_DEP);
                } else {
                    depInfo.setDependency(DepInfo::INPUT_INDEP);
                    // clear argument deps
                    depInfo.setArgumentDependencies(ArgumentSet());
                }
                clonedArgDeps.insert(std::make_pair(argdep_entry.first, depInfo));
            }
            cloned_depInfo.addCall(mapped_instr, clonedArgDeps);
            cloned_depInfo.addCall(mapped_instr, FunctionCallDepInfo::GlobalVariableDependencyMap());
        }
        clonned_call_dep_info.insert(std::make_pair(F, cloned_depInfo));
    }
    clonedResults->setFunctionCallDepInfo(std::move(clonned_call_dep_info));
    return clonedResults;
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
    std::for_each(m_F->arg_begin(), m_F->arg_end(),
            [this] (llvm::Argument& arg) {
                this->m_inputs.push_back(&arg);
                llvm::Value* val = llvm::dyn_cast<llvm::Value>(&arg);
                if (val->getType()->isPointerTy()) {
                    m_outArgDependencies.insert(
                            std::make_pair(&arg, ValueDepInfo(arg.getType(), DepInfo(DepInfo::INPUT_ARGDEP, ArgumentSet{&arg}))));
                }
            });
}

FunctionAnaliser::Impl::DependencyAnalysisResultT
FunctionAnaliser::Impl::createBasicBlockAnalysisResult(llvm::BasicBlock* B, const DepInfo& depInfo)
{
    assert(depInfo.isDefined());
    if (depInfo.isInputIndep()) {
        return DependencyAnalysisResultT(
                new BasicBlockAnalysisResult(m_F, *m_AAR, *m_virtualCallsInfo, *m_indirectCallsInfo, m_inputs, m_FAGetter, B));
    }
    return DependencyAnalysisResultT(
           new NonDeterministicBasicBlockAnaliser(m_F, *m_AAR, *m_virtualCallsInfo, *m_indirectCallsInfo, m_inputs, m_FAGetter, B, depInfo));
}

LoopAnalysisResult* FunctionAnaliser::Impl::createLoopAnalysisResult(const DepInfo& depInfo, llvm::Loop* loop)
{
    LoopAnalysisResult* loopA = new LoopAnalysisResult(m_F, *m_AAR,
                                                       *m_postDomTree,
                                                       *m_virtualCallsInfo,
                                                       *m_indirectCallsInfo,
                                                       m_inputs,
                                                       m_FAGetter,
                                                       *loop,
                                                       *m_LI);
    if (depInfo.isDefined()) {
        loopA->setLoopDependencies(depInfo);
    }
    return loopA;
}

DepInfo FunctionAnaliser::Impl::getBasicBlockPredecessorInstructionsDeps(llvm::BasicBlock* B) const
{
    DepInfo dep(DepInfo::DepInfo::INPUT_INDEP);
    bool postdominates_all_predecessors = true;
    const auto& b_node = (*m_postDomTree)[B];
    auto pred = pred_begin(B);
    while (pred != pred_end(B)) {
        auto pb = *pred;
        const auto& termInstr = pb->getTerminator();
        if (termInstr == nullptr) {
            dep.setDependency(DepInfo::DepInfo::INPUT_ARGDEP);
            break;
        }
        const auto& BBA = getAnalysisResult(pb);
        if (BBA == nullptr) {
            // means either block is in a loop, or cfg is broken. For the first case is safe to continue.
            // For the second case throw exception or continue based on run configuration
            // TODO: is it safe for loop case to assert that the loop of pred is the same as for B?
            if (!m_LI->getLoopFor(pb)
                && !InputDepConfig::get().is_goto_unsafe()
                && !BasicBlocksUtils::get().isBlockUnreachable(pb)) {
                // use stringstream to build message
                std::string msg = B->getName();
                msg += " predecessor ";
                msg +=  pb->getName();
                msg += " has not been analyzed.";
                llvm::dbgs() << msg << "\n";
                throw IrregularCFGException(msg);
            }
            ++pred;
            continue;
        }
        dep.mergeDependencies(BBA->getInstructionDependencies(termInstr));
        auto pred_node = (*m_postDomTree)[*pred];
        postdominates_all_predecessors &= m_postDomTree->dominates(b_node, pred_node);
        ++pred;
    }
    // if block postdominates all its predecessors, it will be reached independent of predecessors.
    llvm::BasicBlock* entry = &m_F->getEntryBlock();
    auto entry_node = (*m_postDomTree)[entry];
    postdominates_all_predecessors &= m_postDomTree->dominates(b_node, entry_node);
    if (postdominates_all_predecessors) {
        return DepInfo(DepInfo::INPUT_INDEP);
    }
    return dep;
}

void FunctionAnaliser::Impl::updateFunctionInputDependencies()
{
    for (const auto& calledF : m_calledFunctions) {
        if (calledF == m_F) {
            continue;
        }
        auto calledFA = m_FAGetter(calledF);
        if (!calledFA) {
            InputDepConfig::get().add_input_dep_function(calledF);
            continue;
        }
        if (m_is_inputDep) {
            if (calledFA && !calledFA->isInputDepFunction()) {
                calledFA->setIsInputDepFunction(true);
            }
            InputDepConfig::get().add_input_dep_function(calledF);
        } else {
            const auto& callDepInfo = getFunctionCallDepInfo(calledF);
            for (const auto& callSite : callDepInfo.getCallSites()) {
                if (isInputDependentBlock(callSite->getParent())) {
                    calledFA->setIsInputDepFunction(true);
                    InputDepConfig::get().add_input_dep_function(calledF);
                }
            }
        }
    }
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
            if (res.first->second.empty()) {
                res.first->second = argDeps;
                continue;
            }
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

void FunctionAnaliser::Impl::updateCalledFunctionsList(const DependencyAnalysisResultT& BAR)
{
    const auto& calledFunctions = BAR->getCallSitesData();
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

DependencyAnaliser::ValueCallbackMap
FunctionAnaliser::Impl::getBasicBlockPredecessorsCallbackFunctions(llvm::BasicBlock* B)
{
    auto pred = pred_begin(B);
    // entry block
    if (pred == pred_end(B)) {
        return DependencyAnaliser::ValueCallbackMap();
    }
    DependencyAnaliser::ValueCallbackMap callbacks;
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
        const auto& pred_callbacks = pos->second->getCallbackFunctions();
        for (const auto& cb : pred_callbacks) {
            auto res = callbacks.insert(cb);
            if (!res.second) {
                res.first->second.insert(cb.second.begin(), cb.second.end());
            }
        }
        ++pred;
    }
    return callbacks;
}

FunctionAnaliser::Impl::DependencyAnalysisResultT FunctionAnaliser::Impl::getAnalysisResult(llvm::BasicBlock* bb) const
{
    assert(bb->getParent() == m_F);
    auto pos = m_BBAnalysisResults.find(bb);
    if (pos != m_BBAnalysisResults.end()) {
        return pos->second;
    }
    if (auto loop = m_LI->getLoopFor(bb)) {
        auto top_loop = Utils::getTopLevelLoop(loop);
        bb = top_loop->getHeader();
    } else {
        auto looppos = m_loopBlocks.find(bb);
        if (looppos != m_loopBlocks.end()) {
            bb = looppos->second;
        }
    }
    pos = m_BBAnalysisResults.find(bb);
    if (pos == m_BBAnalysisResults.end()) {
        const auto& bb_node = (*m_postDomTree)[bb];
        // TODO: commented so that the log is not a mess. uncomment later
        //llvm::dbgs() << "No analysis result for " << bb->getName() << " in function " << m_F->getName() << "\n";
        //if (!m_postDomTree.isReachableFromEntry(bb_node)) {
        //    llvm::dbgs() << "block is not reachable from entry, and is not analyzed.\n";
        //}
        return nullptr;
    }
    assert(pos != m_BBAnalysisResults.end());
    return pos->second;
}

FunctionAnaliser::FunctionAnaliser(llvm::Function* F,
                                   const FunctionAnalysisGetter& getter)
    : m_analiser(new Impl(F, getter))
{
}

void FunctionAnaliser::setFunction(llvm::Function* F)
{
    m_analiser->setFunction(F);
}

void FunctionAnaliser::setAAResults(llvm::AAResults* AAR)
{
    m_analiser->setAAResults(AAR);
}

void FunctionAnaliser::setLoopInfo(llvm::LoopInfo* LI)
{
    m_analiser->setLoopInfo(LI);
}

void FunctionAnaliser::setPostDomTree(const llvm::PostDominatorTree* PDom)
{
    m_analiser->setPostDomTree(PDom);
}

void FunctionAnaliser::setDomTree(const llvm::DominatorTree* dom)
{
    m_analiser->setDomTree(dom);
}

void FunctionAnaliser::setVirtualCallSiteAnalysisResult(const VirtualCallSiteAnalysisResult* virtualCallsInfo)
{
    m_analiser->setVirtualCallSiteAnalysisResult(virtualCallsInfo);
}

void FunctionAnaliser::setIndirectCallSiteAnalysisResult(const IndirectCallSitesAnalysisResult* indirectCallsInfo)
{
    m_analiser->setIndirectCallSiteAnalysisResult(indirectCallsInfo);
}

void FunctionAnaliser::analyze()
{
    m_analiser->analyze();
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

bool FunctionAnaliser::changeFunctionCall(const llvm::Instruction* callInstr, llvm::Function* oldF, llvm::Function* newF)
{
    m_analiser->changeFunctionCall(const_cast<llvm::Instruction*>(callInstr), oldF, newF);
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

bool FunctionAnaliser::isInputDependentBlock(llvm::BasicBlock* block) const
{
    return m_analiser->isInputDependentBlock(block);
}

bool FunctionAnaliser::isControlDependent(llvm::Instruction* I) const
{
    return m_analiser->isControlDependent(I);
}

bool FunctionAnaliser::isDataDependent(llvm::Instruction* I) const
{
    return m_analiser->isDataDependent(I);
}

bool FunctionAnaliser::isArgumentDependent(llvm::Instruction* I) const
{
    return m_analiser->isArgumentDependent(I);
}

bool FunctionAnaliser::isArgumentDependent(llvm::BasicBlock* block) const
{
    return m_analiser->isArgumentDependent(block);
}

bool FunctionAnaliser::isGlobalDependent(llvm::Instruction* I) const
{
    return m_analiser->isGlobalDependent(I);
}

bool FunctionAnaliser::isOutArgInputIndependent(llvm::Argument* arg) const
{
    return m_analiser->isOutArgInputIndependent(arg);
}

ValueDepInfo FunctionAnaliser::getOutArgDependencies(llvm::Argument* arg) const
{
    return m_analiser->getOutArgDependencies(arg);
}

bool FunctionAnaliser::isReturnValueInputIndependent() const
{
    return m_analiser->isReturnValueInputIndependent();
}

const ValueDepInfo& FunctionAnaliser::getRetValueDependencies() const
{
    return m_analiser->getRetValueDependencies();
}

bool FunctionAnaliser::hasGlobalVariableDepInfo(llvm::GlobalVariable* global) const
{
    return m_analiser->hasGlobalVariableDepInfo(global);
}

ValueDepInfo FunctionAnaliser::getGlobalVariableDependencies(llvm::GlobalVariable* global) const
{
    return m_analiser->getGlobalVariableDependencies(global);
}

ValueDepInfo FunctionAnaliser::getDependencyInfoFromBlock(llvm::Value* val, llvm::BasicBlock* block) const
{
    return m_analiser->getDependencyInfoFromBlock(val, block);
}

DepInfo FunctionAnaliser::getBlockDependencyInfo(llvm::BasicBlock* block) const
{
    return m_analiser->getBlockDependencyInfo(block);
}

const GlobalsSet& FunctionAnaliser::getReferencedGlobals() const
{
    return m_analiser->getReferencedGlobals();
}

const GlobalsSet& FunctionAnaliser::getModifiedGlobals() const
{
    return m_analiser->getModifiedGlobals();
}

long unsigned FunctionAnaliser::get_input_dep_blocks_count() const
{
    return m_analiser->get_input_dep_blocks_count();
}

long unsigned FunctionAnaliser::get_input_indep_blocks_count() const
{
    return m_analiser->get_input_indep_blocks_count();
}

long unsigned FunctionAnaliser::get_unreachable_blocks_count() const
{
    return m_analiser->get_unreachable_blocks_count();
}

long unsigned FunctionAnaliser::get_unreachable_instructions_count() const
{
    return m_analiser->get_unreachable_instructions_count();
}

long unsigned FunctionAnaliser::get_input_dep_count() const
{
    return m_analiser->get_input_dep_count();
}

long unsigned FunctionAnaliser::get_input_indep_count() const
{
    return m_analiser->get_input_indep_count();
}

long unsigned FunctionAnaliser::get_data_indep_count() const
{
    return m_analiser->get_data_indep_count();
}

long unsigned FunctionAnaliser::get_input_unknowns_count() const
{
    return m_analiser->get_input_unknowns_count();
}

FunctionInputDependencyResultInterface*
FunctionAnaliser::cloneForArguments(const DependencyAnaliser::ArgumentDependenciesMap& inputDepArgs)
{
    m_analiser->cloneForArguments(inputDepArgs);
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

bool FunctionAnaliser::isInputDepFunction() const
{
    return m_analiser->isInputDepFunction();
}

void FunctionAnaliser::setIsInputDepFunction(bool isInputDep)
{
    m_analiser->setIsInputDepFunction(isInputDep);
}

bool FunctionAnaliser::isExtractedFunction() const
{
    return m_analiser->isExtractedFunction();
}

void FunctionAnaliser::setIsExtractedFunction(bool isExtracted)
{
    m_analiser->setIsExtractedFunction(isExtracted);
}

bool FunctionAnaliser::areArgumentsFinalized() const
{
    return m_analiser->areArgumentsFinalized();
}

bool FunctionAnaliser::areGlobalsFinalized() const
{
    return m_analiser->areGlobalsFinalized();
}


} // namespace input_dependency

