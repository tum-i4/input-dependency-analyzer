#include "input-dependency/Transforms/Utils.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/CFG.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"


#include <list>

namespace oh {

namespace {

void blocks_in_range(llvm::BasicBlock* block,
                     llvm::BasicBlock* stop_block,
                     std::unordered_set<llvm::BasicBlock*>& blocks)
{
    if (block == stop_block) {
        return;
    }
    auto res = blocks.insert(block);
    if (!res.second) {
        return; // block is added, hence successors are also processed
    }
    auto it = succ_begin(block);
    while (it != succ_end(block)) {
        blocks_in_range(*it, stop_block, blocks);
        ++it;
    }
}

}

llvm::BasicBlock::iterator Utils::get_instruction_pos(llvm::Instruction* I)
{
    auto it = I->getParent()->begin();
    while (&*it != I && it != I->getParent()->end()) {
        ++it;
    }
    assert(it != I->getParent()->end());
    return it;
}

llvm::Function::iterator Utils::get_block_pos(llvm::BasicBlock* block)
{
    auto it = block->getParent()->begin();
    while (&*it != block && it != block->getParent()->end()) {
        ++it;
    }
    return it;
}

unsigned Utils::get_instruction_index(const llvm::Instruction* I)
{
    unsigned idx = 0;
    auto it = I->getParent()->begin();
    while (&*it != I && it != I->getParent()->end()) {
        ++it;
        ++idx;
    }
    return idx;
}

std::unordered_set<llvm::BasicBlock*> Utils::get_blocks_in_range(llvm::Function::iterator begin,
                                                                 llvm::Function::iterator end)
{
    std::unordered_set<llvm::BasicBlock*> blocks;
    blocks_in_range(&*begin, &*end, blocks);
    return blocks;
}

std::vector<llvm::BasicBlock*> Utils::get_blocks_in_bfs(llvm::Function::iterator begin, llvm::Function::iterator end)
{
    std::unordered_set<llvm::BasicBlock*> processed_blocks;
    std::list<llvm::BasicBlock*> work_list;
    std::vector<llvm::BasicBlock*> blocks;
    if (&*begin == &*end) {
        return blocks;
    }
    work_list.push_back(&*begin);
    while (!work_list.empty()) {
        llvm::BasicBlock* block = work_list.front();
        work_list.pop_front();
        if (!processed_blocks.insert(block).second) {
            continue;
        }
        if (block == &*end) {
            continue;
        }
        blocks.insert(blocks.begin(), block);
        auto it = succ_begin(block);
        while (it != succ_end(block)) {
            work_list.push_back(*it);
            ++it;
        }
    }
    return blocks;
}

unsigned Utils::get_function_instrs_count(llvm::Function& F)
{
    unsigned count = 0;
    for (auto& B : F) {
        count += B.getInstList().size();
    }
    return count;
}

void Utils::check_module(const llvm::Module& M)
{
    llvm::dbgs() << "Check Module " << M.getName() << "\n";
    for (auto& F : M) {
        unsigned ret_count = 0;
        llvm::dbgs() << "---Function: " << F.getName() << "\n";
        llvm::dbgs() << F << "\n";
        for (auto& B : F) {
            auto terminator = B.getTerminator();
            if (!terminator) {
                llvm::dbgs() << "-----Invalid Block. No Terminator " << B.getName() << "\n"; 
                llvm::dbgs() << B << "\n\n";
            } else if (auto retInstr = llvm::dyn_cast<llvm::ReturnInst>(terminator)) {
                //llvm::dbgs() << "Ret instr: " << *retInstr << "\n";
                ++ret_count;
                auto* returnValue = retInstr->getReturnValue();
                if (returnValue) {
                    if (returnValue->getType()->getTypeID() != F.getReturnType()->getTypeID()) {
                        llvm::dbgs() << "Invalid return type. Return type " << *F.getReturnType()
                            << ". Return value type: " << *returnValue->getType() << "\n";
                        llvm::dbgs() << F << "\n";
                    }
                } else if (retInstr->getType()->isVoidTy() && !F.getReturnType()->isVoidTy()) {
                    llvm::dbgs() << "Invalid return type. Return type " << *F.getReturnType()
                        << ". Return value type: " << *retInstr->getType() << "\n";
                        llvm::dbgs() << F << "\n";
                } else if (!retInstr->getType()->isVoidTy() && F.getReturnType()->isVoidTy()) {
                    llvm::dbgs() << "Invalid return type. Return type " << *F.getReturnType()
                        << ". Return value type: " << *retInstr->getType() << "\n";
                        llvm::dbgs() << F << "\n";
                }
            }
            for (auto& I : B) {
                for (auto& op : I.operands()) {
                    if (!op) {
                        llvm::dbgs() << "Null operand in function \n";
                        llvm::dbgs() << F;
                        assert(false);
                    }
                }
                auto* term_instr = llvm::dyn_cast<llvm::TerminatorInst>(&I);
                if (term_instr && term_instr != terminator) {
                    llvm::dbgs() << "Terminator found in the middle of a basic block! " << B.getName() << "\n";
                    if (terminator) {
                        llvm::dbgs() << *term_instr << "------- " << *terminator << "\n";
                    }
                    llvm::dbgs() << F << "\n";
                    return;
                }
            }
        }
        if (ret_count > 1) {
            llvm::dbgs() << F << "\n";
            llvm::dbgs() << "more than one ret\n";
        }
    }
}

}

;
