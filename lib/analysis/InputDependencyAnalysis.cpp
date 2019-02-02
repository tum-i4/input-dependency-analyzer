#include "analysis/InputDependencyAnalysis.h"

#include "analysis/InputDepInfo.h"
#include "analysis/ReachabilityAnalysis.h"
#include "analysis/InputDependencyReachabilityAnalysis.h"
#include "analysis/InputIndependencyReachabilityAnalysis.h"
#include "analysis/ArgumentReachabilityAnalysis.h"
#include "PDG/InputDependencyNode.h"
#include "utils/Utils.h"

#include "PDG/PDG/PDG.h"
#include "PDG/PDG/FunctionPDG.h"
#include "PDG/LLVMNode.h"
#include "PDG/PDG/PDG.h"
#include "PDG/PDG/PDGNode.h"
#include "PDG/PDG/PDGEdge.h"
#include "PDG/PDG/PDGLLVMNode.h"

#include "llvm/ADT/SCCIterator.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CallGraphSCCPass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"

namespace input_dependency {

InputDependencyAnalysis::InputDependencyAnalysis(llvm::Module* module)
    : m_module(module)
{
}

void InputDependencyAnalysis::setPDG(PDGType pdg)
{
    m_pdg = pdg;
}

void InputDependencyAnalysis::setCallGraph(llvm::CallGraph* callGraph)
{
    m_callGraph = callGraph;
}

/*
* Run InputDependencyReachabilityAnalysis in top-bottom direction with collecting argument dep info at call sites
* Run ArgumentReachabilityAnalysis in bottom-up direction with collecting argument dep info at call sites
* TODO: think if this step is needed: Based on collected data run InputDependencyAnalysis in top-down direction using argument dep info collected earlier
*/
void InputDependencyAnalysis::analyze()
{
    collectFunctionsInBottomUp();
    runInputReachabilityAnalysis();
    setArgumentDependencies();
    runArgumentReachabilityAnalysis();
    runInputIndependencyReachabilityAnalysis();
    //TODO: global reachability
}

bool InputDependencyAnalysis::isInputDependent(llvm::Function* F, llvm::Instruction* instr) const
{
    auto Fpdg = m_pdg->getFunctionPDG(F);
    auto* instrNode = llvm::dyn_cast<LLVMNode>(Fpdg->getNode(instr).get());
    return instrNode->getInputDepInfo().isInputDep();
}

bool InputDependencyAnalysis::isInputDependent(llvm::Instruction* instr) const
{
    return isInputDependent(instr->getFunction(), instr);
}

bool InputDependencyAnalysis::isInputDependent(llvm::BasicBlock* block) const
{
    auto Fpdg = m_pdg->getFunctionPDG(block->getParent());
    auto* blockNode = llvm::dyn_cast<LLVMBasicBlockNode>(Fpdg->getNode(block).get());
    return blockNode->getInputDepInfo().isInputDep();
}

bool InputDependencyAnalysis::isInputDependent(llvm::Function* F) const
{
    auto Fnode = m_pdg->getFunctionNode(F);
    auto* node = llvm::dyn_cast<LLVMFunctionNode>(Fnode.get());
    return node->getInputDepInfo().isInputDep();
}

bool InputDependencyAnalysis::isInputIndependent(llvm::Instruction* I) const
{
    auto Fpdg = m_pdg->getFunctionPDG(I->getFunction());
    auto* instrNode = llvm::dyn_cast<LLVMNode>(Fpdg->getNode(I).get());
    return instrNode->getInputDepInfo().isInputIndep();
}

bool InputDependencyAnalysis::isControlDependent(llvm::Instruction* I) const
{
    auto Fpdg = m_pdg->getFunctionPDG(I->getFunction());
    auto* instrNode = llvm::dyn_cast<LLVMNode>(Fpdg->getNode(I).get());
    return instrNode->getCFInputDepInfo().isInputDep();
}

bool InputDependencyAnalysis::isDataDependent(llvm::Instruction* I) const
{
    auto Fpdg = m_pdg->getFunctionPDG(I->getFunction());
    auto* instrNode = llvm::dyn_cast<LLVMNode>(Fpdg->getNode(I).get());
    return instrNode->getDFInputDepInfo().isInputDep();
}

bool InputDependencyAnalysis::isArgumentDependent(llvm::Instruction* I) const
{
    auto Fpdg = m_pdg->getFunctionPDG(I->getFunction());
    auto* instrNode = llvm::dyn_cast<LLVMNode>(Fpdg->getNode(I).get());
    return instrNode->getInputDepInfo().isArgumentDep();
}

bool InputDependencyAnalysis::isArgumentDependent(llvm::BasicBlock* B) const
{
    auto Fpdg = m_pdg->getFunctionPDG(B->getParent());
    auto* blockNode = llvm::dyn_cast<LLVMBasicBlockNode>(Fpdg->getNode(B).get());
    return blockNode->getInputDepInfo().isArgumentDep();
}

bool InputDependencyAnalysis::isGlobalDependent(llvm::Instruction* I) const
{
    auto Fpdg = m_pdg->getFunctionPDG(I->getFunction());
    auto* instrNode = llvm::dyn_cast<LLVMNode>(Fpdg->getNode(I).get());
    return instrNode->getInputDepInfo().isGlobalDep();
}

unsigned InputDependencyAnalysis::getInputIndepInstrCount(llvm::Function* F) const
{
    auto pos = m_functionDbgInfo.find(F);
    if (pos == m_functionDbgInfo.end()) {
        const_cast<InputDependencyAnalysis*>(this)->collectFunctionDebugInfo(F);
        return m_functionDbgInfo.find(F)->second.InputIndepInstrCount;
    }
    return pos->second.InputIndepInstrCount;
}

unsigned InputDependencyAnalysis::getInputIndepBlocksCount(llvm::Function* F) const
{
    auto pos = m_functionDbgInfo.find(F);
    if (pos == m_functionDbgInfo.end()) {
        const_cast<InputDependencyAnalysis*>(this)->collectFunctionDebugInfo(F);
        return m_functionDbgInfo.find(F)->second.InputIndepBlocksCount;
    }
    return pos->second.InputIndepBlocksCount;
}

unsigned InputDependencyAnalysis::getInputDepInstrCount(llvm::Function* F) const
{
    auto pos = m_functionDbgInfo.find(F);
    if (pos == m_functionDbgInfo.end()) {
        const_cast<InputDependencyAnalysis*>(this)->collectFunctionDebugInfo(F);
        return m_functionDbgInfo.find(F)->second.InputDepInstrCount;
    }
    return pos->second.InputDepInstrCount;
}

unsigned InputDependencyAnalysis::getInputDepBlocksCount(llvm::Function* F) const
{
    auto pos = m_functionDbgInfo.find(F);
    if (pos == m_functionDbgInfo.end()) {
        const_cast<InputDependencyAnalysis*>(this)->collectFunctionDebugInfo(F);
        return m_functionDbgInfo.find(F)->second.InputDepBlocksCount;
    }
    return pos->second.InputDepBlocksCount;
}

unsigned InputDependencyAnalysis::getDataIndepInstrCount(llvm::Function* F) const
{
    auto pos = m_functionDbgInfo.find(F);
    if (pos == m_functionDbgInfo.end()) {
        const_cast<InputDependencyAnalysis*>(this)->collectFunctionDebugInfo(F);
        return m_functionDbgInfo.find(F)->second.DataIndepInstrCount;
    }
    return pos->second.DataIndepInstrCount;
}

unsigned InputDependencyAnalysis::getArgumentDepInstrCount(llvm::Function* F) const
{
    auto pos = m_functionDbgInfo.find(F);
    if (pos == m_functionDbgInfo.end()) {
        const_cast<InputDependencyAnalysis*>(this)->collectFunctionDebugInfo(F);
        return m_functionDbgInfo.find(F)->second.ArgumentDepInstrCount;
    }
    return pos->second.ArgumentDepInstrCount;
}

unsigned InputDependencyAnalysis::getUnreachableBlocksCount(llvm::Function* F) const
{
    auto pos = m_functionDbgInfo.find(F);
    if (pos == m_functionDbgInfo.end()) {
        const_cast<InputDependencyAnalysis*>(this)->collectFunctionDebugInfo(F);
        return m_functionDbgInfo.find(F)->second.UnreachableBlocksCount;
    }
    return pos->second.UnreachableBlocksCount;
}

unsigned InputDependencyAnalysis::getUnreachableInstructionsCount(llvm::Function* F) const
{
    auto pos = m_functionDbgInfo.find(F);
    if (pos == m_functionDbgInfo.end()) {
        const_cast<InputDependencyAnalysis*>(this)->collectFunctionDebugInfo(F);
        return m_functionDbgInfo.find(F)->second.UnreachableInstructionsCount;
    }
    return pos->second.UnreachableInstructionsCount;
}

void InputDependencyAnalysis::runArgumentReachabilityAnalysis()
{
    const ReachabilityAnalysis::NodeProcessor nodeProcessor
        = std::bind(&InputDependencyAnalysis::updateFunctionArgDeps, this, std::placeholders::_1);
    llvm::scc_iterator<llvm::CallGraph*> CGI = llvm::scc_begin(m_callGraph);
    llvm::CallGraphSCC CurSCC(*m_callGraph, &CGI);
    FunctionSet processedFs;
    while (!CGI.isAtEnd()) {
        // Copy the current SCC and increment past it so that the pass can hack
        // on the SCC if it wants to without invalidating our iterator.
        const std::vector<llvm::CallGraphNode *> &NodeVec = *CGI;
        CurSCC.initialize(NodeVec);

        for (llvm::CallGraphNode* node : CurSCC) {
            llvm::Function* F = node->getFunction();
            if (F == nullptr
                    || Utils::isLibraryFunction(F, m_module)
                    || !m_pdg->hasFunctionPDG(F)
                    || !processedFs.insert(F).second) {
                continue;
            }
            m_functions.push_back(F);
            auto Fpdg = m_pdg->getFunctionPDG(F);
            ArgumentReachabilityAnalysis argReach(Fpdg);
            argReach.setNodeProcessor(nodeProcessor);
            argReach.analyze();
        }
        ++CGI;
    }
}

void InputDependencyAnalysis::runInputReachabilityAnalysis()
{
    const ReachabilityAnalysis::NodeProcessor nodeProcessor
        = std::bind(&InputDependencyAnalysis::updateFunctionArgDeps, this, std::placeholders::_1);
    InputDependencyReachabilityAnalysis inputReachAnalysis(m_pdg);
    inputReachAnalysis.setNodeProcessor(nodeProcessor);
    inputReachAnalysis.analyze();
}

void InputDependencyAnalysis::collectFunctionsInBottomUp()
{
    llvm::scc_iterator<llvm::CallGraph*> CGI = llvm::scc_begin(m_callGraph);
    llvm::CallGraphSCC CurSCC(*m_callGraph, &CGI);
    FunctionSet processedFs;
    while (!CGI.isAtEnd()) {
        // Copy the current SCC and increment past it so that the pass can hack
        // on the SCC if it wants to without invalidating our iterator.
        const std::vector<llvm::CallGraphNode *> &NodeVec = *CGI;
        CurSCC.initialize(NodeVec);
        for (llvm::CallGraphNode* node : CurSCC) {
            llvm::Function* F = node->getFunction();
            if (F == nullptr
                    || Utils::isLibraryFunction(F, m_module)
                    || !m_pdg->hasFunctionPDG(F)
                    || !processedFs.insert(F).second) {
                continue;
            }
            m_functions.push_back(F);
        }
        ++CGI;
    }
}

void InputDependencyAnalysis::runInputIndependencyReachabilityAnalysis()
{
    InputIndependencyReachabilityAnalysis(m_pdg.get()).analyze();
}

void InputDependencyAnalysis::setArgumentDependencies()
{
    for (auto* F : m_functions) {
        auto Fpdg = m_pdg->getFunctionPDG(F);
        auto FargDeps = m_functionArgDeps.find(F);
        if (FargDeps == m_functionArgDeps.end()) {
            continue;
        }
        for (auto arg_it = Fpdg->formalArgBegin();
             arg_it != Fpdg->formalArgEnd();
             ++arg_it) {
             auto* argNode = llvm::dyn_cast<LLVMFormalArgumentNode>((*arg_it).second.get());
             auto* arg = llvm::dyn_cast<llvm::Argument>(argNode->getNodeValue());
             unsigned argIdx = arg->getArgNo();
             argNode->mergeDFInputDepInfo(FargDeps->second[argIdx]);
        }
    }
}

void InputDependencyAnalysis::updateFunctionArgDeps(NodeType node)
{
    auto* inputDepNode = llvm::dyn_cast<LLVMNode>(node.get());
    if (!inputDepNode) {
        return;
    }
    auto* actualArg = llvm::dyn_cast<LLVMActualArgumentNode>(node.get());
    if (!actualArg) {
        return;
    }
    const auto& DFinputDepInfo = inputDepNode->getDFInputDepInfo();
    const auto& CFinputDepInfo = inputDepNode->getCFInputDepInfo();
    unsigned argIdx = actualArg->getArgIndex();
    const auto& functions = getCalledFunction(actualArg->getCallSite());
    for (const auto& F : functions) {
        auto [item, inserted] = m_functionArgDeps.insert(std::make_pair(F, ArgInputDependencies()));
        if (inserted) {
            item->second.resize(F->arg_size());
        }
        if (argIdx >= item->second.size()) {
            item->second.resize(argIdx + 1);
        }
        item->second[argIdx].mergeDependencies(DFinputDepInfo);
        item->second[argIdx].mergeDependencies(CFinputDepInfo);
    }
}

InputDependencyAnalysis::FunctionSet
InputDependencyAnalysis::getCalledFunction(const llvm::CallSite& callSite)
{
    FunctionSet callees;
    if (!m_pdg->hasFunctionPDG(callSite.getCaller())) {
        return callees;
    }
    auto callerPdg = m_pdg->getFunctionPDG(callSite.getCaller());
    auto callSiteNode = callerPdg->getNode(callSite.getInstruction());
    // callees are found by traversing control edges with FunctionNode in sink
    for (auto edge_it = callSiteNode->outEdgesBegin();
         edge_it != callSiteNode->outEdgesEnd();
         ++edge_it) {
        if (!(*edge_it)->isControlEdge()) {
            continue;
        }
        auto dest = (*edge_it)->getDestination();
        if (auto* functionNode = llvm::dyn_cast<LLVMFunctionNode>(dest.get())) {
            callees.insert(functionNode->getFunction());
        }
    }
    return callees;
}

void InputDependencyAnalysis::collectFunctionDebugInfo(llvm::Function* F)
{
    DebugInfo dbgInfo;
    for (auto& B : *F) {
        if (isInputDependent(&B)) {
            ++dbgInfo.InputDepBlocksCount;
        } else {
            ++dbgInfo.InputIndepBlocksCount;
        }
        for (auto& I : B) {
            if (isInputDependent(F, &I)) {
                ++dbgInfo.InputDepInstrCount;
                if (!isDataDependent(&I)) {
                    ++dbgInfo.DataIndepInstrCount;
                }
            } else {
                ++dbgInfo.InputIndepInstrCount;
            }
            if (isArgumentDependent(&I)) {
                ++dbgInfo.ArgumentDepInstrCount;
            }
        }
    }
    // TODO: compute these too
    //unsigned UnreachableBlocksCount;
    //unsigned UnreachableInstructionsCount
    m_functionDbgInfo.insert(std::make_pair(F, dbgInfo));
}

} // namespace input_dependency

