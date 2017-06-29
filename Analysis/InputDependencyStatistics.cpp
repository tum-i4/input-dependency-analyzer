#include "InputDependencyStatistics.h"
#include "InputDependencyAnalysis.h"
#include "Utils.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/PassRegistry.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"


namespace input_dependency {

namespace {

void print_stats(const std::string& function_name, unsigned deps, unsigned indeps, unsigned unknowns)
{
    llvm::dbgs() << function_name << "\n";
    llvm::dbgs() << "----------------------\n";
    llvm::dbgs() << "Input Dependent instructions: " << deps << "\n";
    llvm::dbgs() << "Input Independent instructions: " << indeps << "\n";
    llvm::dbgs() << "Unknown instructions: " << unknowns << "\n";
    unsigned percent = (deps * 100) / (deps + indeps + unknowns);
    llvm::dbgs() << "Input dependent instructions' percent: " << percent << "%\n";
}

}

char InputDependencyStatisticsPass::ID = 0;

void InputDependencyStatisticsPass::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.addRequired<InputDependencyAnalysis>();
    AU.setPreservesAll();
}

bool InputDependencyStatisticsPass::runOnModule(llvm::Module& M)
{
    unsigned module_dep_count = 0;
    unsigned module_indep_count = 0;
    unsigned module_unknown_count = 0;
    const auto& IDA = getAnalysis<InputDependencyAnalysis>();
    for (auto& F : M) {
        unsigned dep_count = 0;
        unsigned indep_count = 0;
        unsigned unknown_count = 0;
        const auto& FA = IDA.getAnalysisInfo(&F);
        if (FA == nullptr) {
            continue;
        }
        for (const auto& B : F) {
            for (const auto& I : B) {
                if (FA->isInputDependent(&I)) {
                    ++dep_count;
                } else if (FA->isInputIndependent(&I)) {
                    ++indep_count;
                } else {
                    llvm::dbgs() << "Unknown: " << I << "\n";
                    ++unknown_count;
                }
            }
        }
        print_stats(F.getName(), dep_count, indep_count, unknown_count);
        module_dep_count += dep_count;
        module_indep_count += indep_count;
        module_unknown_count += unknown_count;
    }
    print_stats(M.getName(), module_dep_count, module_indep_count, module_unknown_count);

    return false;
}

static llvm::RegisterPass<InputDependencyStatisticsPass> X("inputdep-statistics","runs input dependency analysis");

}

