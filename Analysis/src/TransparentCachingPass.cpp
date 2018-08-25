#include "input-dependency/Analysis/TransparentCachingPass.h"
#include "input-dependency/Analysis/InputDependencyAnalysisPass.h"
#include "input-dependency/Analysis/InputDependencyAnalysisInterface.h"
#include "input-dependency/Analysis/FunctionInputDependencyResultInterface.h"
#include "input-dependency/Analysis/BasicBlocksUtils.h"

#include "input-dependency/Analysis/Utils.h"
#include "input-dependency/Analysis/constants.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace input_dependency {

void TransparentCachingPass::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.addRequired<InputDependencyAnalysisPass>();
    AU.addPreserved<InputDependencyAnalysisPass>();
    AU.setPreservesAll();
}

bool TransparentCachingPass::runOnModule(llvm::Module& M)
{
    auto IDA = getAnalysis<InputDependencyAnalysisPass>().getInputDependencyAnalysis();
    const auto& functionAnalisers = IDA->getAnalysisInfo();

    M.addModuleFlag(llvm::Module::ModFlagBehavior::Error, metadata_strings::cached_input_dep, true);
    auto* input_dep_function_md_str = llvm::MDString::get(M.getContext(), metadata_strings::input_dep_function);
    llvm::MDNode* input_dep_function_md = llvm::MDNode::get(M.getContext(), input_dep_function_md_str);
    auto* input_indep_function_md_str = llvm::MDString::get(M.getContext(), metadata_strings::input_indep_function);
    llvm::MDNode* input_indep_function_md = llvm::MDNode::get(M.getContext(), input_indep_function_md_str);
    auto* extracted_function_md_str = llvm::MDString::get(M.getContext(), metadata_strings::extracted);
    llvm::MDNode* extracted_function_md = llvm::MDNode::get(M.getContext(), extracted_function_md_str);

    auto* input_dep_block_md_str = llvm::MDString::get(M.getContext(), metadata_strings::input_dep_block);
    llvm::MDNode* input_dep_block_md = llvm::MDNode::get(M.getContext(), input_dep_block_md_str);
    auto* input_indep_block_md_str = llvm::MDString::get(M.getContext(), metadata_strings::input_indep_block);
    llvm::MDNode* input_indep_block_md = llvm::MDNode::get(M.getContext(), input_indep_block_md_str);

    auto* input_dep_instr_md_str = llvm::MDString::get(M.getContext(), metadata_strings::input_dep_instr);
    llvm::MDNode* input_dep_instr_md = llvm::MDNode::get(M.getContext(), input_dep_instr_md_str);
    auto* input_indep_instr_md_str = llvm::MDString::get(M.getContext(), metadata_strings::input_indep_instr);
    llvm::MDNode* input_indep_instr_md = llvm::MDNode::get(M.getContext(), input_indep_instr_md_str);

    auto* control_dep_instr_md_str = llvm::MDString::get(M.getContext(), metadata_strings::control_dep_instr);
    llvm::MDNode* control_dep_instr_md = llvm::MDNode::get(M.getContext(), control_dep_instr_md_str);
    auto* data_dep_instr_md_str = llvm::MDString::get(M.getContext(), metadata_strings::data_dep_instr);
    llvm::MDNode* data_dep_instr_md = llvm::MDNode::get(M.getContext(), data_dep_instr_md_str);
    auto* data_indep_instr_md_str = llvm::MDString::get(M.getContext(), metadata_strings::data_indep_instr);
    llvm::MDNode* data_indep_instr_md = llvm::MDNode::get(M.getContext(), data_indep_instr_md_str);

    auto* global_dep_instr_md_str = llvm::MDString::get(M.getContext(), metadata_strings::global_dep_instr);
    llvm::MDNode* global_dep_instr_md = llvm::MDNode::get(M.getContext(), global_dep_instr_md_str);
    auto* arg_dep_instr_md_str = llvm::MDString::get(M.getContext(), metadata_strings::argument_dep_instr);
    llvm::MDNode* arg_dep_instr_md = llvm::MDNode::get(M.getContext(), arg_dep_instr_md_str);

    auto* unknown_node_name = llvm::MDString::get(M.getContext(), metadata_strings::unknown);
    llvm::MDNode* unknown_md = llvm::MDNode::get(M.getContext(), unknown_node_name);
    auto* unreachable_node_name = llvm::MDString::get(M.getContext(), metadata_strings::unreachable);
    llvm::MDNode* unreachable_md = llvm::MDNode::get(M.getContext(), unreachable_node_name);


    for (auto& FA_item : functionAnalisers) {
        llvm::Function* F = FA_item.first;
        auto& FA = FA_item.second;
        llvm::dbgs() << "Caching input dependenct for function " << F->getName() << "\n";
        if (FA->isInputDepFunction()) {
            F->setMetadata(metadata_strings::input_dep_function, input_dep_function_md);
        } else {
            F->setMetadata(metadata_strings::input_indep_function, input_indep_function_md);
        }
        if (FA->isExtractedFunction()) {
            F->setMetadata(metadata_strings::extracted, extracted_function_md);
        }
        for (auto& B : *F) {
            bool is_input_dep_block = false;
            if (FA->isInputDependentBlock(&B)) {
                is_input_dep_block = true;
                B.begin()->setMetadata(metadata_strings::input_dep_block, input_dep_block_md);
                // don't add metadata_strings to instructions as they'll all be input dep
            } else if (BasicBlocksUtils::get().isBlockUnreachable(&B)) {
                B.begin()->setMetadata(metadata_strings::unreachable, unreachable_md);
                continue;
            } else {
                B.begin()->setMetadata(metadata_strings::input_indep_block, input_indep_block_md);
            }
            for (auto& I : B) {
                if (!is_input_dep_block) {
                    if (FA->isInputDependent(&I)) {
                        I.setMetadata(metadata_strings::input_dep_instr, input_dep_instr_md);
                    } else if (FA->isInputIndependent(&I)) {
                        I.setMetadata(metadata_strings::input_indep_instr, input_indep_instr_md);
                    } else {
                        I.setMetadata(metadata_strings::unknown, unknown_md);
                    }
                }
                if (FA->isControlDependent(&I)) {
                    I.setMetadata(metadata_strings::control_dep_instr, control_dep_instr_md);
                }
                if (FA->isGlobalDependent(&I)) {
                    I.setMetadata(metadata_strings::global_dep_instr, global_dep_instr_md);
                }
                if (FA->isArgumentDependent(&I)) {
                    I.setMetadata(metadata_strings::argument_dep_instr, arg_dep_instr_md);
                }
                if (FA->isDataDependent(&I)) {
                    I.setMetadata(metadata_strings::data_dep_instr, data_dep_instr_md);
                } else {
                    I.setMetadata(metadata_strings::data_indep_instr, data_indep_instr_md);
                }
            }
        }
    }

}

char TransparentCachingPass::ID = 0;
static llvm::RegisterPass<TransparentCachingPass> X("transparent-cache","Cache input dependency results");
}

