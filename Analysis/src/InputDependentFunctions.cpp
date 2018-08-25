#include "input-dependency/Analysis/InputDependentFunctions.h"

#include "input-dependency/Analysis/FunctionDominanceTree.h"
#include "input-dependency/Analysis/InputDependencyAnalysis.h"
#include "input-dependency/Analysis/IndirectCallSitesAnalysis.h"
#include "input-dependency/Analysis/Utils.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CallGraphSCCPass.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/InstrTypes.h"

namespace input_dependency {

char InputDependentFunctionsPass::ID = 0;

void InputDependentFunctionsPass::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.setPreservesAll();
    AU.setPreservesCFG();
    AU.addRequired<IndirectCallSitesAnalysis>();
    AU.addRequired<InputDependencyAnalysisPass>();
    AU.addRequired<FunctionDominanceTreePass>();
    AU.addRequired<llvm::CallGraphWrapperPass>();
    AU.addPreserved<llvm::CallGraphWrapperPass>();
}

std::unordered_set<llvm::Function*> get_call_targets(llvm::CallInst* callInst,
                                                     const IndirectCallSitesAnalysisResult& indirectCallSitesInfo)
{
    std::unordered_set<llvm::Function*> indirectTargets;
    auto calledF = callInst->getCalledFunction();
    if (calledF != nullptr) {
        indirectTargets.insert(calledF);
    } else if (indirectCallSitesInfo.hasIndirectCallTargets(callInst)) {
        indirectTargets = indirectCallSitesInfo.getIndirectCallTargets(callInst);
    }
    return indirectTargets;
}

std::unordered_set<llvm::Function*> get_invoke_targets(llvm::InvokeInst* callInst,
                                                     const IndirectCallSitesAnalysisResult& indirectCallSitesInfo)
{
    std::unordered_set<llvm::Function*> indirectTargets;
    auto calledF = callInst->getCalledFunction();
    if (calledF != nullptr) {
        indirectTargets.insert(calledF);
    } else if (indirectCallSitesInfo.hasIndirectInvokeTargets(callInst)) {
        indirectTargets = indirectCallSitesInfo.getIndirectInvokeTargets(callInst);
    }
    return indirectTargets;
}

void InputDependentFunctionsPass::erase_from_deterministic_functions(const FunctionSet& targets)
{
    for (const auto& target : targets) {
        functions_called_in_det_blocks.erase(target);
    }
}


void InputDependentFunctionsPass::process_non_det_block(llvm::BasicBlock& block,
                                              const IndirectCallSitesAnalysisResult& indirectCallSitesInfo)
{
    std::unordered_set<llvm::Function*> targets;
    for (auto& I : block) {
        targets.clear();
        if (auto* callInst = llvm::dyn_cast<llvm::CallInst>(&I)) {
            targets = get_call_targets(callInst, indirectCallSitesInfo);
        } else if (auto* invokeInst = llvm::dyn_cast<llvm::InvokeInst>(&I)) {
            targets = get_invoke_targets(invokeInst, indirectCallSitesInfo);
        }
        if (!targets.empty()) {
            functions_called_in_non_det_blocks.insert(targets.begin(), targets.end());
            erase_from_deterministic_functions(targets);
        }
    }
}

void InputDependentFunctionsPass::process_call(llvm::Function* parentF,
                                    const FunctionSet& targets,
                                    const IndirectCallSitesAnalysisResult& indirectCallSitesInfo,
                                    const InputDependencyAnalysis& inputDepAnalysis,
                                    const FunctionDominanceTree& domTree,
                                    FunctionSet& processed_functions)
{
    bool is_non_det_caller = (functions_called_in_non_det_blocks.find(parentF) != functions_called_in_non_det_blocks.end());
    if (is_non_det_caller) {
        functions_called_in_non_det_blocks.insert(targets.begin(), targets.end());
        erase_from_deterministic_functions(targets);
        return;
    }
    auto domNode = domTree.get_function_dominators(parentF);
    auto& dominators = domNode->get_dominators();
    for (auto& dom : dominators) {
        if (is_non_det_caller) {
            break;
        }
        auto dom_F = dom->get_function();
        if (functions_called_in_non_det_blocks.find(dom_F) != functions_called_in_non_det_blocks.end()) {
            is_non_det_caller = true;
            break;
        } else {
            if (dom_F == parentF) {
                continue;
            }
            process_function(dom_F, indirectCallSitesInfo, inputDepAnalysis, domTree, processed_functions);
            assert(processed_functions.find(dom_F) != processed_functions.end());
            is_non_det_caller = functions_called_in_non_det_blocks.find(dom_F) != functions_called_in_non_det_blocks.end();
        }
    }
    if (is_non_det_caller) {
        functions_called_in_non_det_blocks.insert(targets.begin(), targets.end());
        erase_from_deterministic_functions(targets);
    } else {
        for (auto& target : targets) {
            if (functions_called_in_non_det_blocks.find(target) == functions_called_in_non_det_blocks.end()) {
                functions_called_in_det_blocks.insert(target);
            }
        }
    }
}

void InputDependentFunctionsPass::process_function(llvm::Function* F,
                                         const IndirectCallSitesAnalysisResult& indirectCallSitesInfo,
                                         const InputDependencyAnalysis& inputDepAnalysis,
                                         const FunctionDominanceTree& domTree,
                                         FunctionSet& processed_functions)
{
    //llvm::dbgs() << "Process function " << F->getName() << "\n";
    if (processed_functions.find(F) != processed_functions.end()) {
        return;
    }
    processed_functions.insert(F);
    for (auto& B : *F) {
        bool is_non_deterministic_block = inputDepAnalysis.isInputDependent(&B);
        if (is_non_deterministic_block) {
            process_non_det_block(B, indirectCallSitesInfo);
            continue;
        }
        for (auto& I : B) {
            if (auto* callInst = llvm::dyn_cast<llvm::CallInst>(&I)) {
                process_call(F, get_call_targets(callInst, indirectCallSitesInfo),
                             indirectCallSitesInfo, inputDepAnalysis, domTree, processed_functions);
            } else if (auto* invokeInst = llvm::dyn_cast<llvm::InvokeInst>(&I)) {
                process_call(F, get_invoke_targets(invokeInst, indirectCallSitesInfo),
                             indirectCallSitesInfo, inputDepAnalysis, domTree, processed_functions);
            }
        }
    }
}

std::vector<llvm::Function*> InputDependentFunctionsPass::collect_functons(llvm::Module& M)
{
    std::vector<llvm::Function*> module_functions;

    llvm::CallGraph& CG = getAnalysis<llvm::CallGraphWrapperPass>().getCallGraph();
    llvm::scc_iterator<llvm::CallGraph*> CGI = llvm::scc_begin(&CG);
    llvm::CallGraphSCC CurSCC(CG, &CGI);
    while (!CGI.isAtEnd()) {
        // Copy the current SCC and increment past it so that the pass can hack
        // on the SCC if it wants to without invalidating our iterator.
        const std::vector<llvm::CallGraphNode *> &NodeVec = *CGI;
        CurSCC.initialize(NodeVec.data(), NodeVec.data() + NodeVec.size());
        for (llvm::CallGraphNode* node : CurSCC) {
            llvm::Function* F = node->getFunction();
            if (F == nullptr || Utils::isLibraryFunction(F, &M)) {
                continue;
            }
            module_functions.insert(module_functions.begin(), F);
        }
        ++CGI;

    }
    return module_functions;
}

bool InputDependentFunctionsPass::runOnModule(llvm::Module& M)
{
    auto module_functions = collect_functons(M);
    const auto& inputDepAnalysis = getAnalysis<InputDependencyAnalysisPass>().getInputDependencyAnalysis();
    const auto& domTree = getAnalysis<FunctionDominanceTreePass>().get_dominance_tree();
    FunctionSet processed_functions;
    for (auto& F : module_functions) {
        if (F->isDeclaration() || F->isIntrinsic()) {
            continue;
        }
        if (F->getName() == "main") {
            functions_called_in_det_blocks.insert(F);
        }
        const auto& indirectCallAnalysis = getAnalysis<IndirectCallSitesAnalysis>();
        const auto& indirectCallSitesInfo = indirectCallAnalysis.getIndirectsAnalysisResult();
        process_function(F, indirectCallSitesInfo, *inputDepAnalysis, domTree, processed_functions);
    }
    // go through others
    for (auto& F : M) {
        if (F.isDeclaration() || F.isIntrinsic()) {
            continue;
        }
        if (processed_functions.find(&F) != processed_functions.end()) {
            continue;
        }
        const auto& indirectCallAnalysis = getAnalysis<IndirectCallSitesAnalysis>();
        const auto& indirectCallSitesInfo = indirectCallAnalysis.getIndirectsAnalysisResult();
        process_function(&F, indirectCallSitesInfo, *inputDepAnalysis, domTree, processed_functions);
    }
    for (auto& F : M) {
        if (functions_called_in_non_det_blocks.find(&F) == functions_called_in_non_det_blocks.end()
            && functions_called_in_det_blocks.find(&F) == functions_called_in_det_blocks.end()) {
            //llvm::dbgs() << "No info for function " << F.getName() << "\n";
        }
    }
    //for (const auto& f : functions_called_in_det_blocks) {
    //    llvm::dbgs() << "Function is called from det block " << f->getName() << "\n";
    //}
    //llvm::dbgs() << "\n";
    //for (const auto& f : functions_called_in_non_det_blocks) {
    //    llvm::dbgs() << "Function is called from NON det block " << f->getName() << "\n";
    //}
    return false;
}

bool InputDependentFunctionsPass::is_function_input_dependent(llvm::Function* F) const
{
    return functions_called_in_non_det_blocks.find(F) != functions_called_in_non_det_blocks.end();
}

bool InputDependentFunctionsPass::is_function_input_independent(llvm::Function* F) const
{
    return functions_called_in_det_blocks.find(F) != functions_called_in_det_blocks.end();
}

static llvm::RegisterPass<InputDependentFunctionsPass> X("function-call-info","Collects information about function calls");
}
