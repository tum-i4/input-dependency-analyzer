#include "input-dependency/Analysis/Utils.h"

#include "llvm/Pass.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/PassRegistry.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "nlohmann/json.hpp"
#include <fstream>
#include <iomanip>

namespace {

class ModuleSizeDebugPass : public llvm::ModulePass
{
public:
    static char ID;

    ModuleSizeDebugPass()
        : llvm::ModulePass(ID)
    {
    }

    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override
    {
        AU.setPreservesAll();
        AU.addRequired<llvm::LoopInfoWrapperPass>();
    }

    bool runOnModule(llvm::Module& M) override
    {
        long unsigned function_count = 0;
        long unsigned block_count = 0;
        long unsigned instruction_count = 0;
        long unsigned loop_count = 0;
        long unsigned loop_instr_count = 0;

        for (auto& F : M) {
            if (F.isIntrinsic() || input_dependency::Utils::isLibraryFunction(&F, &M)) {
                continue;
            }
            llvm::LoopInfo& LI = getAnalysis<llvm::LoopInfoWrapperPass>(F).getLoopInfo();
            ++function_count;
            block_count += F.getBasicBlockList().size();
            for (const auto& B : F) {
                auto loop = LI.getLoopFor(&B);
                if (loop != nullptr && loop->getHeader() == &B) {
                    ++loop_count;
                }
                if (loop != nullptr) {
                    loop_instr_count += B.getInstList().size();
                }
                instruction_count += B.getInstList().size();
            }
        }
        nlohmann::json root;
        root[M.getName()]["functions"] = function_count;
        root[M.getName()]["basic_blocks"] = block_count;
        root[M.getName()]["loops"] = loop_count;
        root[M.getName()]["instructions"] = instruction_count;
        root[M.getName()]["loop_instructions"] = loop_instr_count;
        std::ofstream strm("module_data");
        strm << std::setw(4) << root;
        strm.close();
        return false;
    }
};

char ModuleSizeDebugPass::ID = 0;

static llvm::RegisterPass<ModuleSizeDebugPass> X("mod-size","reports module size in terms of function count, basic block count and instruction count");

}

