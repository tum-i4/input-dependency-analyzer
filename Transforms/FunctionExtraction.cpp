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
#include "Analysis/InputDependencyAnalysis.h"
#include "Analysis/InputDependentFunctions.h"

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
    bool derive_input_dependency_from_args(llvm::Instruction* I) const;
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
        ++it;
    }
    if (expand) {
        expand_snippets();
    }
}

void SnippetsCreator::expand_snippets()
{
    for (auto& snippet : m_snippets) {
        //snippet->dump();
        snippet->expand();
        //snippet->dump();
    }
    if (m_snippets.size() == 1) {
        if ((*m_snippets.begin())->is_single_instr_snippet()) {
            m_snippets.clear();
        }
    }

    auto it = m_snippets.begin();
    while (it != m_snippets.end()) {
        if ((*it)->to_blockSnippet()) {
            // do not merge blocks' snippet. may improve later
            ++it;
            continue;
        }
        auto next_it = it + 1;
        if (next_it == m_snippets.end()) {
            // last snippet and is one instruction snippet
            if ((*it)->is_single_instr_snippet()) {
                m_snippets.erase(it);
            }
            break;
        }
        if ((*it)->intersects(**next_it)) {
            (*next_it)->merge(**it);
            auto old = it;
            ++it;
            m_snippets.erase(old);
        } else if ((*it)->is_single_instr_snippet()) {
            // e.g. load of input dep pointer, to store input indep value to it
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
        //llvm::dbgs() << "instr " << *I << "\n";
        bool is_input_dep = m_input_dep_info->isInputDependent(I);
        if (!is_input_dep) {
            // TODO: what other instructions might be intresting?
            if (auto* callInst = llvm::dyn_cast<llvm::CallInst>(I)) {
                if (callInst->getFunctionType()->getReturnType()->isVoidTy()) {
                    is_input_dep = derive_input_dependency_from_args(I);
                }
            }
        }
        if (!is_input_dep) {
            if (InstructionsSnippet::is_valid_snippet(begin, end, block)) {
                snippets.push_back(Snippet_type(new InstructionsSnippet(block, begin, end)));
                begin = block->end();
                end = block->end();
            }
        } else {
            if (auto store = llvm::dyn_cast<llvm::StoreInst>(I)) {
                // Skip the instruction storing argument to a local variable. This should happen anyway, no need to extract
                if (llvm::dyn_cast<llvm::Argument>(store->getValueOperand())) {
                    ++it;
                    continue;
                }
            } else if (auto ret = llvm::dyn_cast<llvm::ReturnInst>(I)) {
                break;
            }
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

bool SnippetsCreator::derive_input_dependency_from_args(llvm::Instruction* I) const
{
    // return true if all arguments are input dependent
    bool is_input_dep = true;
    for (unsigned i = 0; i < I->getNumOperands(); ++i) {
        auto op = I->getOperand(i);
        if (auto op_inst = llvm::dyn_cast<llvm::Instruction>(op)) {
            is_input_dep = m_input_dep_info->isInputDependent(op_inst);
            if (!is_input_dep) {
                break;
            }
        }
    }
    return is_input_dep;
}

bool SnippetsCreator::can_root_blocks_snippet(llvm::BasicBlock* block) const
{
    auto terminator = block->getTerminator();
    if (!m_input_dep_info->isInputDependent(terminator)) {
        return false;
    }
    auto branch = llvm::dyn_cast<llvm::BranchInst>(terminator);
    if (branch) {
        return branch->isConditional();
    }
    auto switch_inst = llvm::dyn_cast<llvm::SwitchInst>(terminator);
    return (switch_inst != nullptr);
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

void run_on_function(llvm::Function& F,
                     llvm::PostDominatorTree* PDom,
                     SnippetsCreator::InputDependencyAnalysisInfo* input_dep_info,
                     std::unordered_set<llvm::Function*>& extracted_functions)
{
    // map from block to snippets?
    SnippetsCreator creator(F);
    creator.set_input_dep_info(input_dep_info);
    creator.set_post_dom_tree(PDom);
    creator.collect_snippets(true);
    const auto& snippets = creator.get_snippets();

    for (auto& snippet : snippets) {
        //snippet->dump();
        auto extracted_function = snippet->to_function();
        extracted_functions.insert(extracted_function);
    }
}

}

void FunctionExtractionPass::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.addRequired<llvm::PostDominatorTreeWrapperPass>();
    AU.addRequired<input_dependency::InputDependencyAnalysis>();
    AU.addRequired<input_dependency::InputDependentFunctionsPass>();
    AU.setPreservesAll();
}

bool FunctionExtractionPass::runOnModule(llvm::Module& M)
{
    bool modified = false;
    auto& input_dep = getAnalysis<input_dependency::InputDependencyAnalysis>();
    const auto& function_calls = getAnalysis<input_dependency::InputDependentFunctionsPass>();

    for (auto& F : M) {
        llvm::dbgs() << "\nStart function extraction on function " << F.getName() << "\n";
        if (F.isDeclaration()) {
            llvm::dbgs() << "Skip: Declaration function " << F.getName() << "\n";
            continue;
        }
        auto input_dep_info = input_dep.getAnalysisInfo(&F);
        if (input_dep_info == nullptr) {
            llvm::dbgs() << "Skip: No input dep info for function " << F.getName() << "\n";
            continue;
        }
        if (!function_calls.is_function_input_independent(&F)) {
            llvm::dbgs() << "Skip: Input dependent function " << F.getName() << "\n";
            continue;
        }
        llvm::PostDominatorTree* PDom = &getAnalysis<llvm::PostDominatorTreeWrapperPass>(F).getPostDomTree();
        run_on_function(F, PDom, input_dep_info, m_extracted_functions);
        modified = true;
        llvm::dbgs() << "Done function extraction on function " << F.getName() << "\n";
    }

    llvm::dbgs() << "\nExtracted functions are \n";
    for (const auto& f : m_extracted_functions) {
        llvm::dbgs() << f->getName() << "\n";
    }
    return modified;
}

char FunctionExtractionPass::ID = 0;
static llvm::RegisterPass<FunctionExtractionPass> X(
                                "extract-functions",
                                "Transformation pass to extract input dependent snippets into separate functions");
}

