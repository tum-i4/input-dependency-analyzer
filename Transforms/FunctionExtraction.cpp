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
#include "Utils.h"
#include "Analysis/InputDependencyStatistics.h"

#include <vector>
#include <memory>

namespace oh {

namespace {

class SnippetsCreator
{
public:
    using InputDependencyAnalysisInfo = input_dependency::FunctionAnaliser;
    using Snippet_type = std::shared_ptr<Snippet>;
    using BasicBlockRange = std::pair<BasicBlocksSnippet::iterator, BasicBlocksSnippet::iterator>;
    using snippet_list = std::vector<Snippet_type>;

public:
    SnippetsCreator(llvm::Function& F)
        : m_F(F)
    {
    }

    void set_input_dep_info(InputDependencyAnalysisInfo* info)
    {
        m_input_dep_info = info;
    }

    void set_post_dom_tree(llvm::PostDominatorTree* pdom)
    {
        m_pdom = pdom;
    }

    const snippet_list& get_snippets() const
    {
        return m_snippets;
    }

public:
    void collect_snippets(bool expand);
    void expand_snippets();

private:
    snippet_list collect_block_snippets(llvm::Function::iterator block_it);
    bool can_root_blocks_snippet(llvm::BasicBlock* block) const;
    BasicBlockRange get_blocks_snippet(llvm::Function::iterator begin_block_pos);
    llvm::BasicBlock* find_block_postdominator(llvm::BasicBlock* block);
    void update_processed_blocks(const llvm::BasicBlock* block,
                                 const llvm::BasicBlock* stop_block,
                                 std::unordered_set<const llvm::BasicBlock*>& processed_blocks);

private:
    llvm::Function& m_F;
    InputDependencyAnalysisInfo* m_input_dep_info;
    llvm::PostDominatorTree* m_pdom;
    std::unordered_map<llvm::BasicBlock*, snippet_list> m_block_snippets;
    snippet_list m_snippets;
};

void SnippetsCreator::collect_snippets(bool expand)
{
    std::unordered_set<const llvm::BasicBlock*> processed_blocks;
    auto it = m_F.begin();
    while (it != m_F.end()) {
        auto B = &*it;
        auto pos = processed_blocks.find(B);
        if (pos != processed_blocks.end()) {
            ++it;
            continue;
        }
        auto block_snippets = collect_block_snippets(it);
        if (!can_root_blocks_snippet(B)) {
            ++it;
            processed_blocks.insert(B);
            m_snippets.insert(m_snippets.end(), block_snippets.begin(), block_snippets.end());
            m_block_snippets[B] = block_snippets;
            continue;
        }
        // assert back end iter is block's terminator
        auto blocks_range = get_blocks_snippet(it);
        if (!BasicBlocksSnippet::is_valid_snippet(blocks_range.first, blocks_range.second, &m_F)) {
            llvm::dbgs() << "Failed to create snippet out of blocks, starting with block "
                         << B->getName() << "\n";
        } else {
            auto back = block_snippets.back();
            block_snippets.pop_back();
            update_processed_blocks(&*blocks_range.first, &*blocks_range.second, processed_blocks);
            Snippet_type blocks_snippet(new BasicBlocksSnippet(&m_F,
                                                               blocks_range.first,
                                                               blocks_range.second,
                                                               *back->to_instrSnippet()));
            block_snippets.push_back(blocks_snippet);
        }
        // for some blocks will run insert twice
        processed_blocks.insert(B);
        m_snippets.insert(m_snippets.end(), block_snippets.begin(), block_snippets.end());
        m_block_snippets[B] = block_snippets;
        ++it;
    }
    if (expand) {
        expand_snippets();
    }
}

void SnippetsCreator::expand_snippets()
{
    for (auto& snippet : m_snippets) {
        snippet->expand();
    }

    auto it = m_snippets.begin();
    while (it != m_snippets.end()) {
        if ((*it)->to_blockSnippet()) {
            // do not merge blocks' snippet. may improve later
            ++it;
            continue;
        }
        auto next_it = it + 1;
        if ((*it)->intersects(**next_it)) {
            (*next_it)->merge(**it);
            auto old = it;
            ++it;
            m_snippets.erase(old);
        } else {
            ++it;
        }
    }
}

SnippetsCreator::snippet_list SnippetsCreator::collect_block_snippets(llvm::Function::iterator block_it)
{
    snippet_list snippets;
    auto block = &*block_it;
    InstructionsSnippet::iterator begin = block->end();
    InstructionsSnippet::iterator end = block->end();
    auto it = block->begin();
    while (it != block->end()) {
        llvm::Instruction* I = &*it;
        if (!m_input_dep_info->isInputDependent(I)) {
            if (InstructionsSnippet::is_valid_snippet(begin, end, block)) {
                snippets.push_back(Snippet_type(new InstructionsSnippet(block, begin, end)));
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
    if (InstructionsSnippet::is_valid_snippet(begin, end, block)) {
        snippets.push_back(Snippet_type(new InstructionsSnippet(block, begin, end)));
    }
    return snippets;
}

bool SnippetsCreator::can_root_blocks_snippet(llvm::BasicBlock* block) const
{
    auto terminator = block->getTerminator();
    if (!m_input_dep_info->isInputDependent(terminator)) {
        return false;
    }
    auto branch = llvm::dyn_cast<llvm::BranchInst>(terminator);
    if (!branch || !branch->isConditional()) {
        return false;
    }
    return true;
}

SnippetsCreator::BasicBlockRange SnippetsCreator::get_blocks_snippet(llvm::Function::iterator begin_block_pos)
{
    auto end_block = find_block_postdominator(&*begin_block_pos);
    llvm::Function::iterator end_block_pos = Utils::get_block_pos(end_block);
    return std::make_pair(begin_block_pos, end_block_pos);
}

llvm::BasicBlock* SnippetsCreator::find_block_postdominator(llvm::BasicBlock* block)
{
    const auto& b_node = (*m_pdom)[block];
    auto F = block->getParent();
    auto block_to_process = block;
    while (block_to_process != nullptr) {
        if (block_to_process != block) {
            auto pr_node = (*m_pdom)[block_to_process];
            if (m_pdom->dominates(pr_node, b_node)) {
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

void SnippetsCreator::update_processed_blocks(const llvm::BasicBlock* block,
                                              const llvm::BasicBlock* stop_block,
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
    llvm::PostDominatorTree* PDom = &getAnalysis<llvm::PostDominatorTreeWrapperPass>().getPostDomTree();

    // map from block to snippets?
    SnippetsCreator creator(F);
    creator.set_input_dep_info(input_dep_info);
    creator.set_post_dom_tree(PDom);
    creator.collect_snippets(true);
    const auto& snippets = creator.get_snippets();

    llvm::dbgs() << "Snippets for function " << F.getName() << "\n";
    for (auto& snippet : snippets) {
        snippet->dump();
    }
    return modified;
}

char FunctionExtractionPass::ID = 0;
static llvm::RegisterPass<FunctionExtractionPass> X(
                                "extract-functions",
                                "Transformation pass to extract input dependent snippets into separate functions");
}

