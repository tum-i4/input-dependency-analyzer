#include "input-dependency/Analysis/ReachableFunctions.h"

#include "input-dependency/Analysis/InputDependencyAnalysisPass.h"
#include "input-dependency/Analysis/InputDependencyAnalysisInterface.h"
#include "input-dependency/Analysis/InputDependencyAnalysis.h"
#include "input-dependency/Analysis/FunctionAnaliser.h"
#include "input-dependency/Analysis/FunctionInputDependencyResultInterface.h"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/PassRegistry.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include <list>

namespace input_dependency {

ReachableFunctions::ReachableFunctions(llvm::Module* M,
                                       llvm::CallGraph* cfg)
    : m_module(M)
    , m_callGraph(cfg)
{
}

void ReachableFunctions::setInputDependencyAnalysisResult(InputDependencyAnalysisInterface* inputDepAnalysis)
{
    m_inputDepAnalysis = inputDepAnalysis;
}

ReachableFunctions::FunctionSet
ReachableFunctions::getReachableFunctions(llvm::Function* F)
{
    FunctionSet reachable_functions;

    llvm::CallGraphNode* entryNode = (*m_callGraph)[F];
    collect_reachable_functions(entryNode,
                                reachable_functions);

    collect_indirectly_reachable_functions(reachable_functions);
    collect_functions_with_uses(reachable_functions);
    return reachable_functions;
}

void ReachableFunctions::collect_reachable_functions(
                                llvm::CallGraphNode* callNode,
                                FunctionSet& reachable_functions)
{
    if (!callNode) {
        return;
    }
    llvm::Function* nodeF = callNode->getFunction();
    if (!nodeF || nodeF->isDeclaration()) {
        return;
    }
    if (!reachable_functions.insert(nodeF).second) {
        return;
    }
    for (auto call_it = callNode->begin(); call_it != callNode->end(); ++call_it) {
        collect_reachable_functions(call_it->second, reachable_functions);
    }
}

void ReachableFunctions::collect_indirectly_reachable_functions(
                                                FunctionSet& reachable_functions)
{
    std::list<llvm::Function*> working_list;
    working_list.insert(working_list.end(), reachable_functions.begin(), reachable_functions.end());
    FunctionSet processed_functions;

    while (!working_list.empty()) {
        auto F = working_list.front();
        working_list.pop_front();
        if (!processed_functions.insert(F).second) {
            continue;
        }
        auto F_inputDepAnalysis = m_inputDepAnalysis->getAnalysisInfo(F);
        if (!F_inputDepAnalysis) {
            continue;
        }
        const auto& calledFunctions = F_inputDepAnalysis->getCallSitesData();
        for (auto& calledF : calledFunctions) {
            if (!reachable_functions.insert(calledF).second) {
                continue;
            }
            working_list.push_back(calledF);
        }
    }
}

void ReachableFunctions::collect_functions_with_uses(FunctionSet& reachable_functions)
{
    std::unordered_map<llvm::Function*, FunctionSet> waiting_list;
    for (auto& F : *m_module) {

        if (F.isDeclaration()) {
            continue;
        }
        if (reachable_functions.find(&F) != reachable_functions.end()) {
            continue;
        }
        if (F.user_empty()) {
            continue;
        }
        for (auto user_it = F.user_begin(); user_it != F.user_end(); ++user_it) {
            const auto& userFunctions = getUserFunctions(*user_it);
            for (auto& user_F : userFunctions) {
                if (reachable_functions.find(user_F) != reachable_functions.end()) {
                    reachable_functions.insert(&F);
                    break;
                }
            }
            if (reachable_functions.find(&F) == reachable_functions.end()) {
                for (auto& user_F : userFunctions) {
                    waiting_list[user_F].insert(&F);
                }
            }
        }
    }
    // TODO: there is a flaw here
    for (auto& item : waiting_list) {
        if (reachable_functions.find(item.first) != reachable_functions.end()) {
            reachable_functions.insert(item.second.begin(), item.second.end());
        }
    }
}

ReachableFunctions::FunctionSet ReachableFunctions::getUserFunctions(llvm::User* user)
{
    FunctionSet functions;
    if (auto* inst = llvm::dyn_cast<llvm::Instruction>(user)) {
        return {inst->getFunction()};
    } else {
        for (auto it = user->user_begin(); it != user->user_end(); ++it) {
            const auto& user_functions = getUserFunctions(*it);
            if (!user_functions.empty()) {
                functions.insert(user_functions.begin(), user_functions.end());
            }
        }

    }
    return functions;
}

char ReachableFunctionsPass::ID = 0;

static llvm::cl::opt<bool> delete_unreachables(
    "delete-unreachables",
    llvm::cl::desc("Delete unreachable functions"),
    llvm::cl::value_desc("boolean flag"));


bool ReachableFunctionsPass::runOnModule(llvm::Module &M)
{
    bool modified = false;
    llvm::CallGraph* CG = &getAnalysis<llvm::CallGraphWrapperPass>().getCallGraph();
    auto IDA = getAnalysis<input_dependency::InputDependencyAnalysisPass>().getInputDependencyAnalysis();
    ReachableFunctions reachableFs(&M, CG);
    reachableFs.setInputDependencyAnalysisResult(IDA.get());
    llvm::Function* mainF = M.getFunction("main");
    if (!mainF) {
        llvm::dbgs() << "No function main\n";
        return modified;
    }
    const auto& reachable_from_main = reachableFs.getReachableFunctions(mainF);
    llvm::dbgs() << "Function reachable from main are\n";
    for (const auto& F : reachable_from_main) {
        llvm::dbgs() << "+++" << F->getName() << "\n";
    }

    llvm::dbgs() << "Non reachable functions\n";
    auto f_it = M.begin();
    while (f_it != M.end()) {
        llvm::Function* F = &*f_it;
        ++f_it;
        if (F->isDeclaration()) {
            continue;
        }
        if (reachable_from_main.find(F) == reachable_from_main.end()) {
            llvm::dbgs() << "---" << F->getName() << "\n";
            if (delete_unreachables) {
                modified = true;
                if (F->user_empty()) {
                    F->eraseFromParent();
                } else {
                    for (auto user_it = F->user_begin(); user_it != F->user_end(); ++user_it) {
                        (*user_it)->dropAllReferences();
                    }
                    F->dropAllReferences();
                    F->eraseFromParent();
                }
            }
        }
    }
    //M.dump();
    return modified;
}

void ReachableFunctionsPass::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
    AU.addRequired<llvm::CallGraphWrapperPass>();
    AU.addRequired<InputDependencyAnalysisPass>();
    AU.setPreservesAll();
}

static llvm::RegisterPass<ReachableFunctionsPass> X("reachables","Find main reachable functions");


}

