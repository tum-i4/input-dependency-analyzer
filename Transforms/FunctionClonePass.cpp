#include "FunctionClonePass.h"

#include "Analysis/InputDependencyStatistics.h"

#include "llvm/ADT/SCCIterator.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

#include "llvm/PassRegistry.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"


namespace oh {

char FunctionClonePass::ID = 0;

void FunctionClonePass::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.addRequired<input_dependency::InputDependencyAnalysis>();
    AU.addRequired<llvm::CallGraphWrapperPass>();
}

bool FunctionClonePass::runOnModule(llvm::Module& M)
{
    bool isChanged = true;
    IDA = &getAnalysis<input_dependency::InputDependencyAnalysis>();
    auto it = M.begin();
    std::unordered_set<llvm::Function*> to_process;
    std::unordered_set<llvm::Function*> processed;
    while (it != M.end()) {
        auto F = &*it;
        if (F->isDeclaration() || F->isIntrinsic()) {
            ++it;
            continue;
        }
        if (processed.find(F) != processed.end()) {
            ++it;
            continue;
        }
        to_process.insert(F);
        while (!to_process.empty()) {
            auto pos = to_process.begin();
            auto& currentF = *pos;
            processed.insert(currentF);
            input_dependency::FunctionAnaliser* analysisInfo;
            auto analysispos = m_duplicatedAnalysisInfo.find(currentF);
            if (analysispos != m_duplicatedAnalysisInfo.end()) {
                analysisInfo = &analysispos->second;
            } else {
                analysisInfo = IDA->getAnalysisInfo(currentF);
            }
            assert(analysisInfo != nullptr);
            const auto& callSites = analysisInfo->getCallSitesData();
            for (const auto& callSite : callSites) {
                if (callSite->isDeclaration() || callSite->isIntrinsic()) {
                    continue;
                }
                const auto& clonedFunctions = doClone(analysisInfo, callSite);
                to_process.insert(clonedFunctions.begin(), clonedFunctions.end());
            }
            to_process.erase(currentF);
        }
        ++it;
    }

    dumpStatistics(M);

    return isChanged;
}

std::unordered_set<llvm::Function*> FunctionClonePass::doClone(const input_dependency::FunctionAnaliser* analiser,
                                                               llvm::Function* calledF)
{
    llvm::dbgs() << "   doClone " << calledF->getName() << "\n";

    std::unordered_set<llvm::Function*> clonedFunctions;
    if (m_functionCloneInfo.find(calledF) == m_functionCloneInfo.end()) {
        FunctionClone clone(calledF);
        m_functionCloneInfo.emplace(calledF, std::move(clone));
    }
    input_dependency::FunctionAnaliser* calledFunctionAnaliser;
    auto pos = m_duplicatedAnalysisInfo.find(calledF);
    if (pos != m_duplicatedAnalysisInfo.end()) {
        calledFunctionAnaliser = &pos->second;
    } else {
        calledFunctionAnaliser = IDA->getAnalysisInfo(calledF);
    }

    auto& clone = m_functionCloneInfo.find(calledF)->second;
    auto functionCallDepInfo = analiser->getFunctionCallDepInfo(calledF);
    auto& callArgDeps = functionCallDepInfo.getCallsArgumentDependencies();
    for (auto& argDepItem : callArgDeps) {
        const FunctionClone::mask& mask = FunctionClone::createMaskForCall(argDepItem.second,
                                                                           calledF->getArgumentList().size(),
                                                                           calledF->isVarArg());
        llvm::Function* F = nullptr;
        if (clone.hasCloneForMask(mask)) {
            F = clone.getClonedFunction(mask);
        } else {
            F = clone.doCloneForMask(mask);
            clonedFunctions.insert(F);
            cloneFunctionAnalysisInfo(calledFunctionAnaliser, F, argDepItem.second);
        }
        changeFunctionCall(argDepItem.first, F);
    }
    return clonedFunctions;
}

void FunctionClonePass::changeFunctionCall(const llvm::Instruction* instr, llvm::Function* F)
{
    if (auto* callInst = llvm::dyn_cast<llvm::CallInst>(instr)) {
        (const_cast<llvm::CallInst*>(callInst))->setCalledFunction(F);
    } else if (auto* invokeInst = llvm::dyn_cast<llvm::InvokeInst>(instr)) {
        (const_cast<llvm::InvokeInst*>(invokeInst))->setCalledFunction(F);
    } else {
        assert(false);
    }
}

void FunctionClonePass::cloneFunctionAnalysisInfo(const input_dependency::FunctionAnaliser* analiser,
                                                  llvm::Function* Fclone,
                                                  const input_dependency::FunctionCallDepInfo::ArgumentDependenciesMap& argumentDeps)
{
    input_dependency::FunctionAnaliser clonedAnaliser(*analiser);
    clonedAnaliser.setFunction(Fclone);
    clonedAnaliser.finalizeArguments(argumentDeps);
    m_duplicatedAnalysisInfo.emplace(Fclone, std::move(clonedAnaliser));
}

void FunctionClonePass::dumpStatistics(llvm::Module& M)
{
    input_dependency::InputDependencyStatistics statistics;
    statistics.report(M, m_duplicatedAnalysisInfo);
}

static llvm::RegisterPass<FunctionClonePass> X(
                                "clone-functions",
                                "Transformation pass to duplicate functions with different set ot input dependent arguments");
}

