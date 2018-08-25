#include "input-dependency/Analysis/LoopTraversalPath.h"
#include "input-dependency/Analysis/Utils.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"


namespace input_dependency {

LoopTraversalPathCreator::LoopTraversalPathCreator(llvm::LoopInfo& LI, llvm::Loop& L)
    : m_LI(LI)
    , m_L(L)
{
}

void LoopTraversalPathCreator::construct()
{
    std::list<llvm::BasicBlock*> blocks;
    std::unordered_set<llvm::BasicBlock*> seen_blocks;

    blocks.push_back(m_L.getHeader());
    while (!blocks.empty()) {
        auto block = blocks.back();
        if (m_uniquify_map.find(block) != m_uniquify_map.end()) {
            blocks.pop_back();
            continue;
        }
        // if seen assume all predecessors has been added
        if (seen_blocks.find(block) == seen_blocks.end()) {
            bool preds_added = add_predecessors(block, blocks);
            if (!preds_added) {
                seen_blocks.insert(block);
                continue;
            }
        }
        add_to_path(block);
        blocks.pop_back();
        add_successors(block, seen_blocks, blocks);
    }
}

bool LoopTraversalPathCreator::add_predecessors(llvm::BasicBlock* block,
                                                LoopPathType& blocks)
{
    auto block_loop = m_LI.getLoopFor(block);
    if (block_loop && block_loop->getHeader() == block && block_loop->getLoopDepth() == 1) {
        return true;
    }
    auto pred = pred_begin(block);
    bool preds_added = true;
    // add notion of seen blocks, not to traverse pred's second time
    while (pred != pred_end(block)) {
        auto pred_loop = m_LI.getLoopFor(*pred);
        if (pred_loop == nullptr) {
            ++pred;
            continue;
        }
        if (m_uniquify_map.find(*pred) != m_uniquify_map.end()) {
            ++pred;
            continue;
        }
        if (pred_loop != &m_L && !m_L.contains(pred_loop)) {
            ++pred;
            continue;
        }
        preds_added = false;
        blocks.push_back(*pred);
        ++pred;
    }
    return preds_added;
}

void LoopTraversalPathCreator::add_successors(llvm::BasicBlock* block,
                                              const std::unordered_set<llvm::BasicBlock*>& seen_blocks,
                                              LoopPathType& blocks)
{
    auto succ = succ_begin(block);
    auto block_loop = m_LI.getLoopFor(block);
    while (succ != succ_end(block)) {
        if (seen_blocks.find(*succ) != seen_blocks.end()) {
            ++succ;
            continue;
        }
        if (m_uniquify_map.find(*succ) != m_uniquify_map.end()) {
            ++succ;
            continue;
        }
        auto succ_loop = m_LI.getLoopFor(*succ);
        if (succ_loop == nullptr) {
            // getLoopFor should be constant time, as denseMap is implemented as hash table 
            // is_loopExiting is not constant
            //assert(m_LI.getLoopFor(block)->isLoopExiting(block));
            ++succ;
            continue;
        }
        if (succ_loop != &m_L) {
            if (succ_loop->getLoopDepth() - m_L.getLoopDepth() > 1) {
                ++succ;
                continue;
            }
        }
        blocks.push_front(*succ);
        ++succ;
    }
}

void LoopTraversalPathCreator::add_to_path(llvm::BasicBlock* block)
{
    auto block_loop = m_LI.getLoopFor(block);
    assert(block_loop != nullptr);
    // comparing header is cheaper than isLoopHeader
    if (block_loop != &m_L && block_loop->getHeader() != block) {
        // assume the header of loop has been processed, hence is in m_uniquify_map
        //assert(m_uniquify_map.find(block_loop->getHeader()) != m_uniquify_map.end());
        return;
    }
    if (block_loop->contains(&m_L) && block_loop != &m_L) {
        return;
    } else if (block_loop->getLoopDepth() - m_L.getLoopDepth() > 1) {
        return;
    } else if (block_loop != &m_L && block_loop->getLoopDepth() == m_L.getLoopDepth()) {
        return;
    }
    
    m_path.push_back(block);
    m_uniquify_map.insert(block);
}

} // namespace input_dependency
