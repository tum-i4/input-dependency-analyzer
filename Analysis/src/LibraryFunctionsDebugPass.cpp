#include "input-dependency/Analysis/LibFunctionInfo.h"
#include "input-dependency/Analysis/LibraryInfoManager.h"
#include "input-dependency/Analysis/Utils.h"

#include "llvm/Pass.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/PassRegistry.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include <fstream>

namespace {

class LibraryFunctionDebugPass : public llvm::ModulePass
{
public:
    static char ID;

    LibraryFunctionDebugPass()
        : llvm::ModulePass(ID)
    {
    }

    bool runOnModule(llvm::Module& M) override
    {
        report_strm.open("library_functions.rep");
        for (auto& F : M) {
            for (auto& B : F) {
                for (auto& I : B) {
                    if (auto callInst = llvm::dyn_cast<llvm::CallInst>(&I)) {
                        llvm::Function* called_f = callInst->getCalledFunction();
                        if (called_f != nullptr) {
                            report_function(called_f);
                        }
                    } else if (auto invokeInst = llvm::dyn_cast<llvm::InvokeInst>(&I)) {
                        llvm::Function* called_f = invokeInst->getCalledFunction();
                        if (called_f != nullptr) {
                            report_function(called_f);
                        }
                    }
                }
            }
        }
        return false;
    }

private:
    void report_function(llvm::Function* F)
    {
        if (!input_dependency::Utils::isLibraryFunction(F, F->getParent())) {
            return;
        }
        if (F->isIntrinsic()) {
            return;
        }
        auto& libInfo = input_dependency::LibraryInfoManager::get();
        auto Fname = input_dependency::Utils::demangle_name(F->getName());
        if (Fname.empty()) {
            // log msg
            // Try with non-demangled name
            Fname = F->getName();
        }
        auto res = added_functions.insert(Fname);
        if (res.second) {
            if (!libInfo.hasLibFunctionInfo(Fname)) {
                report_strm << Fname << "\n";
            }
        }
    }

private:
    std::ofstream report_strm;
    std::unordered_set<std::string> added_functions;
};

char LibraryFunctionDebugPass::ID = 0;

static llvm::RegisterPass<LibraryFunctionDebugPass> X("lib-func-report","reports library functions not configured");

}

