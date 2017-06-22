#include "InputDependencyDebugInfoPrinter.h"

#include "InputDependencyAnalysis.h"


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

namespace {

void log_instruction(llvm::Instruction& instr, std::ofstream& log_stream)
{
    const auto& debug_loc = instr.getDebugLoc();
    if (debug_loc.get() == nullptr) {
        llvm::dbgs() << "No debug info for instruction " << instr << "\n";
        return;
    }
    auto file = debug_loc.get()->getScope()->getFile();
    const std::string file_name = file->getFilename();
    log_stream << "file: " << file_name
               << " line: "
               << debug_loc.getLine()
               << " column: "
               << debug_loc.getCol() << "\n";

    //llvm::dbgs() << instr << "\n";
}

}


char InputDependencyDebugInfoPrinterPass::ID = 0;

void InputDependencyDebugInfoPrinterPass::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.setPreservesAll();
    AU.addRequired<InputDependencyAnalysis>();
}

bool InputDependencyDebugInfoPrinterPass::runOnModule(llvm::Module& M)
{
    auto& inputDepRes = getAnalysis<InputDependencyAnalysis>();

    const std::string module_name = M.getName();
    std::string file_name = module_name + "_dbginfo";
    std::ofstream dbg_infostrm;
    dbg_infostrm.open(file_name);

    for (auto& F : M) {
        auto funcInputDep = inputDepRes.getAnalysisInfo(&F);
        if (funcInputDep == nullptr) {
            llvm::dbgs() << "No input dependency info for function " << F.getName() << " in module " << module_name << "\n";
            continue;
        }
        for (auto& B : F) {
            for (auto& I : B) {
                if (funcInputDep->isInputDependent(&I)) {
                    log_instruction(I, dbg_infostrm);
                }
            }
        }
    }
    dbg_infostrm.close();

    return false;
}

static llvm::RegisterPass<InputDependencyDebugInfoPrinterPass> X("inputdep-dbginfo","Dumps input dependent instructions' debug info");

}

