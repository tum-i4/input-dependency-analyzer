#include "input-dependency/Analysis/CFGTraversalPath.h"
#include "input-dependency/Analysis/Utils.h"
#include "input-dependency/Analysis/BasicBlocksUtils.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <unordered_set>


namespace input_dependency {

CFGTraversalPathCreator::CFGTraversalPathCreator(llvm::Function& F)
    : m_F(F)
{
}

void CFGTraversalPathCreator::setLoopInfo(llvm::LoopInfo* LI)
{
    m_LI = LI;
}

void CFGTraversalPathCreator::setDomTree(const llvm::DominatorTree* domTree)
{
    m_domTree = domTree;
}

const CFGTraversalPathCreator::BlocksInTraversalOrder& CFGTraversalPathCreator::getBlocksInOrder() const
{
    return m_blockOrder;
}

CFGTraversalPathCreator::BlocksInTraversalOrder& CFGTraversalPathCreator::getBlocksInOrder()
{
    return m_blockOrder;
}

const CFGTraversalPathCreator::BlockToLoopMap& CFGTraversalPathCreator::getBlocksLoops() const
{
    return m_loopBlocks;
}

CFGTraversalPathCreator::BlockToLoopMap& CFGTraversalPathCreator::getBlocksLoops()
{
    return m_loopBlocks;
}

void CFGTraversalPathCreator::construct(mode in_mode)
{
    if (in_mode == mode::SCC) {
        construct_with_scc();
    } else if (in_mode == mode::CFG) {
        construct_with_cfg();
    } else {
        assert(false);
    }
}

void CFGTraversalPathCreator::construct_with_scc()
{
    typedef llvm::scc_iterator<llvm::BasicBlock*> BB_scc_iterator;;
    BlocksInTraversalOrder blocks_in_order;
    llvm::BasicBlock* block = &m_F.front();
    auto it = BB_scc_iterator::begin(block);
    llvm::BasicBlock* bb = nullptr;
    llvm::Loop* currentLoop;
    while (it != BB_scc_iterator::end(block)) {
        const auto& scc_blocks = *it;
        bool is_loop_block = false;
        bb = *scc_blocks.begin();
        if (scc_blocks.size() > 1) {
            is_loop_block = true;
            llvm::Loop* bb_loop = m_LI->getLoopFor(bb);
            if (!bb_loop) {
                llvm::dbgs() << "SCC node with multiple blocks, not constructing a loop\n";
                for (const auto& b : scc_blocks) {
                    llvm::Loop* loop = m_LI->getLoopFor(b);
                    if (!loop) {
                        //llvm::dbgs() << "no loop for block " << b->getName() << ". adding as single block\n";
                        m_blockOrder.push_front(std::make_pair(b, nullptr));
                    } else {
                        while (loop && loop->getLoopDepth() != 1) {
                            loop = loop->getParentLoop();
                        }
                        if (!loop) {
                            // strange, see if this can happen
                            llvm::dbgs() << "no loop for block " << b->getName() << ". adding as single block\n";
                        } else {
                            //llvm::dbgs() << "add block " << b->getName() << " to loop with header " <<
                            //loop->getHeader()->getName() << "\n";
                            m_loopBlocks[loop->getHeader()] = b;
                            m_blockOrder.push_front(std::make_pair(b, loop));
                        }
                    }
                }
                ++it;
                continue;
            }
            while (bb_loop && bb_loop->getLoopDepth() != 1) {
                bb_loop = bb_loop->getParentLoop();
            }
            if (currentLoop == nullptr) {
                currentLoop = bb_loop;
            } else if (currentLoop == bb_loop || currentLoop->contains(bb_loop)) {
                ++it;
                continue;
            } else {
                currentLoop = bb_loop;
            }
            if (currentLoop == nullptr) {
                llvm::dbgs() << "Error: expecting loop for " << bb->getName() << "\n";
                llvm::dbgs() << "skipping block\n";
                ++it;
                continue;
            } else {
                bb = currentLoop->getHeader();
            }
        }
        if (is_loop_block) {
            for (auto& scc_b : scc_blocks) {
                //llvm::dbgs() << "Add loop block " << scc_b->getName() << " loop: " << bb->getName() << "\n";
                //llvm::dbgs() << "       " << (*scc_blocks.begin())->getName() << "\n";
                m_loopBlocks[scc_b] = bb;
            }
        }
        m_blockOrder.push_front(std::make_pair(bb, is_loop_block ? currentLoop : nullptr));
        ++it;
    }
}

void CFGTraversalPathCreator::construct_with_cfg()
{
    std::list<llvm::BasicBlock*> work_list;
    std::unordered_set<llvm::BasicBlock*> processed_blocks;
    work_list.push_front(&m_F.getEntryBlock());
    std::vector<llvm::BasicBlock*> waiting_blocks;
    while (!work_list.empty()) {
        llvm::BasicBlock* block = work_list.front();
        //llvm::dbgs() << "block: " << block->getName() << "\n";
        work_list.pop_front();
        if (!processed_blocks.insert(block).second) {
            continue;
        }
        llvm::Loop* loop = m_LI->getLoopFor(block);
        while (loop && loop->getLoopDepth() != 1) {
            loop = loop->getParentLoop();
        }
        if (loop) {
            m_loopBlocks[block] = loop->getHeader();
            if (block == loop->getHeader()) {
                m_blockOrder.push_back(std::make_pair(block, loop));
            }
        } else {
            m_blockOrder.push_back(std::make_pair(block, nullptr));
        }
        auto succ_it = succ_begin(block);
        while (succ_it != succ_end(block)) {
            //llvm::dbgs() << "succ: " << (*succ_it)->getName() << "\n";
            auto pred_it = pred_begin(*succ_it);
            bool add_successor = true;
            while (pred_it != pred_end(*succ_it)) {
                auto pred = *pred_it;
                //llvm::dbgs() << "pred: " << pred->getName() << "\n";
                if (processed_blocks.find(pred) != processed_blocks.end()) {
                    ++pred_it;
                    continue;
                }
                auto pred_loop = m_LI->getLoopFor(pred);
                if (pred_loop && pred_loop->getHeader() == *succ_it) {
                    ++pred_it;
                    continue;
                }
                // predecessor is unreachable, don't wait for it to be analyzed
                if (isBlockUnreachable(pred)) {
                    ++pred_it;
                    continue;
                }
                waiting_blocks.push_back(*succ_it);
                add_successor = false;
                break;
            }
            if (add_successor) {
                work_list.push_back(*succ_it);
            }
            ++succ_it;
        }
        if (work_list.empty() && !waiting_blocks.empty()) {
            work_list.insert(work_list.end(), waiting_blocks.begin(), waiting_blocks.end());
            waiting_blocks.clear();
        }
    }
}

bool CFGTraversalPathCreator::isBlockUnreachable(llvm::BasicBlock* block)
{
    if (BasicBlocksUtils::get().isBlockUnreachable(block)) {
        return true;
    }
    if (!m_domTree->isReachableFromEntry(block)) {
        BasicBlocksUtils::get().addUnreachableBlock(block);
        return true;
    }
    return false;
}

} // namespace input_dependency

