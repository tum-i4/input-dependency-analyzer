#include "FunctionExtraction.h"

#include "llvm/ADT/SCCIterator.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"

#include "llvm/PassRegistry.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "FunctionSnippet.h"
#include "Analysis/InputDependencyStatistics.h"

#include <vector>
#include <memory>

namespace oh {

namespace {

using InputDependencyAnalysisInfo = input_dependency::FunctionAnaliser;
using Snippet_type = std::shared_ptr<Snippet>;
using BasicBlockRange = std::pair<BasicBlocksSnippet::iterator, BasicBlocksSnippet::iterator>;
using snippet_list = std::vector<Snippet_type>;

bool is_valid_snippet(InstructionsSnippet::iterator begin,
                      InstructionsSnippet::iterator end,
                      llvm::BasicBlock* block)
{
    bool valid = (begin != block->end());
    valid &= (begin->getParent() == block);
    if (end != block->end()) {
        valid &= (end->getParent() == block);
    }
    return valid;
}

bool is_valid_snippet(BasicBlocksSnippet::iterator begin,
                      BasicBlocksSnippet::iterator end,
                      llvm::Function* parent)
{
    return (begin != parent->end() && begin != end);
}

// TODO: check if llvm has function returning iterator for given block
llvm::Function::iterator get_block_pos(llvm::BasicBlock* block)
{
    auto it = block->getParent()->begin();
    while (&*it != block && it != block->getParent()->end()) {
        ++it;
    }
    return it;
}

llvm::BasicBlock* find_block_postdominator(llvm::BasicBlock* block, const llvm::PostDominatorTree& PDom)
{
    const auto& b_node = PDom[block];
    auto F = block->getParent();
    auto block_to_process = block;
    while (block_to_process != nullptr) {
        if (block_to_process != block) {
            auto pr_node = PDom[block_to_process];
            if (PDom.dominates(pr_node, b_node)) {
                break;
            }
        }
        auto succ = succ_begin(block_to_process);
        if (succ == succ_end(block_to_process)) {
            // block_to_process is the exit block
            block_to_process = nullptr;
        } else {
            block_to_process = *succ;
        }
    }
    assert(block_to_process != nullptr);
    return block_to_process;
}

BasicBlockRange get_blocks_snippet(llvm::Function::iterator begin_block_pos, const llvm::PostDominatorTree& PDom)
{
    auto end_block = find_block_postdominator(&*begin_block_pos, PDom);
    llvm::Function::iterator end_block_pos = get_block_pos(end_block);
    return std::make_pair(begin_block_pos, end_block_pos);
}

snippet_list collect_block_snippets(llvm::Function::iterator block_it,
                                    const InputDependencyAnalysisInfo& input_dep_info,
                                    const llvm::PostDominatorTree& PDom)
{
    snippet_list snippets;
    auto block = &*block_it;
    InstructionsSnippet::iterator begin = block->end();
    InstructionsSnippet::iterator end = block->end();
    auto it = block->begin();
    while (it != block->end()) {
        llvm::Instruction* I = &*it;
        if (!input_dep_info.isInputDependent(I)) {
            if (is_valid_snippet(begin, end, block)) {
                snippets.push_back(Snippet_type(new InstructionsSnippet(begin, end)));
                begin = block->end();
                end = block->end();
            }
        } else {
            if (begin != block->end()) {
                end = it;
            } else {
                begin = it;
                end = it;
            }
        }
        ++it;
    }
    if (is_valid_snippet(begin, end, block)) {
        snippets.push_back(Snippet_type(new InstructionsSnippet(begin, end)));
    }
    return snippets;
}

bool can_root_blocks_snippet(llvm::BasicBlock* block,
                             const InputDependencyAnalysisInfo& input_dep_info)
{
    auto terminator = block->getTerminator();
    if (!input_dep_info.isInputDependent(terminator)) {
        return false;
    }
    auto branch = llvm::dyn_cast<llvm::BranchInst>(terminator);
    if (!branch || !branch->isConditional()) {
        return false;
    }
    return true;
}

void update_processed_blocks(const llvm::BasicBlock* block, const llvm::BasicBlock* stop_block,
                             std::unordered_set<const llvm::BasicBlock*>& processed_blocks)
{
    if (block == stop_block) {
        return;
    }
    auto res = processed_blocks.insert(block);
    if (!res.second) {
        return; // block is added, hence successors are also processed
    }
    auto it = succ_begin(block);
    while (it != succ_end(block)) {
        update_processed_blocks(*it, stop_block, processed_blocks);
        ++it;
    }
}

}

void FunctionExtractionPass::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.addRequired<llvm::PostDominatorTreeWrapperPass>();
    AU.addRequired<input_dependency::InputDependencyAnalysis>();
    AU.setPreservesAll();
}

bool FunctionExtractionPass::runOnFunction(llvm::Function& F)
{
    bool modified = false;
    auto input_dep_info = getAnalysis<input_dependency::InputDependencyAnalysis>().getAnalysisInfo(&F);
    assert(input_dep_info != nullptr);
    const llvm::PostDominatorTree& PDom = getAnalysis<llvm::PostDominatorTreeWrapperPass>().getPostDomTree();

    // map from block to snippets?
    std::vector<Snippet_type> snippets;
    std::unordered_set<const llvm::BasicBlock*> processed_blocks;

    auto it = F.begin();
    while (it != F.end()) {
        auto B = &*it;
        auto pos = processed_blocks.find(B);
        if (pos != processed_blocks.end()) {
            ++it;
            continue;
        }
        auto block_snippets = collect_block_snippets(it, *input_dep_info, PDom);

        if (!can_root_blocks_snippet(B, *input_dep_info)) {
            ++it;
            processed_blocks.insert(B);
            snippets.insert(snippets.end(), block_snippets.begin(), block_snippets.end());
            continue;
        }
        // assert back end iter is block's terminator
        auto blocks_range = get_blocks_snippet(it, PDom);
        if (!is_valid_snippet(blocks_range.first, blocks_range.second, &F)) {
            llvm::dbgs() << "Failed to create snippet out of blocks, starting with block "
                         << B->getName() << "\n";
        } else {
            auto back = block_snippets.back();
            block_snippets.pop_back();
            update_processed_blocks(&*blocks_range.first, &*blocks_range.second, processed_blocks);
            Snippet_type blocks_snippet(new BasicBlocksSnippet(blocks_range.first,
                                                               blocks_range.second,
                                                               *back->to_instrSnippet()));
            block_snippets.push_back(blocks_snippet);
        }
        // for some blocks will run insert twice
        processed_blocks.insert(B);
        snippets.insert(snippets.end(), block_snippets.begin(), block_snippets.end());
        ++it;
    }

    llvm::dbgs() << "Snippets for function " << F.getName() << "\n";
    for (const auto& snippet : snippets) {
        snippet->dump();
    }
    llvm::dbgs() << "\n";
    return modified;
}

char FunctionExtractionPass::ID = 0;
static llvm::RegisterPass<FunctionExtractionPass> X(
                                "extract-functions",
                                "Transformation pass to extract input dependent snippets into separate functions");
}

