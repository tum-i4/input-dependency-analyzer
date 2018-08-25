#include "input-dependency/Analysis/InputDependencyDebugInfoPrinter.h"

#include "input-dependency/Analysis/InputDependencyAnalysisPass.h"
#include "input-dependency/Analysis/InputDependencyAnalysis.h"
#include "input-dependency/Analysis/InputDepInstructionsRecorder.h"
#include "input-dependency/Analysis/FunctionInputDependencyResultInterface.h"
#include "input-dependency/Analysis/LoggingUtils.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/PassRegistry.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include <fstream>

namespace input_dependency {

char InputDependencyDebugInfoPrinterPass::ID = 0;

void InputDependencyDebugInfoPrinterPass::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.setPreservesAll();
    AU.addRequired<InputDependencyAnalysisPass>();
}

bool InputDependencyDebugInfoPrinterPass::runOnModule(llvm::Module& M)
{
    auto inputDepRes = getAnalysis<InputDependencyAnalysisPass>().getInputDependencyAnalysis();

    std::string module_name = M.getName();
    auto dot_pos = module_name.find_first_of('.');
    module_name = module_name.substr(0, dot_pos);
    std::string file_name = module_name + ".dbg";
    std::ofstream dbg_infostrm;
    dbg_infostrm.open(file_name);

    InputDepInstructionsRecorder& recorder = InputDepInstructionsRecorder::get();
    recorder.set_record();

    LoggingUtils logger;
    for (auto& F : M) {
        if (F.isDeclaration() || F.isIntrinsic()) {
            continue;
        }
        auto funcInputDep = inputDepRes->getAnalysisInfo(&F);
        if (funcInputDep == nullptr) {
            llvm::dbgs() << "No input dependency info for function " << F.getName() << " in module " << module_name << "\n";
            continue;
        }
        llvm::dbgs() << F.getName() << "\n";
        for (auto& B : F) {
            if (inputDepRes->isInputDependent(&B)) {
                recorder.record(&B);
            }
            for (auto& I : B) {
                if (funcInputDep->isInputDependent(&I)) {
                    logger.log_instruction_dbg_info(I, dbg_infostrm);
                }
            }
        }
    }
    logger.log_not_logged_count(dbg_infostrm);
    recorder.dump_dbg_info();
    dbg_infostrm.close();

    return false;
}

static llvm::RegisterPass<InputDependencyDebugInfoPrinterPass> X("inputdep-dbginfo","Dumps input dependent instructions' debug info");

}

