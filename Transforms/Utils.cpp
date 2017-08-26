#include "Utils.h"

namespace oh {

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

unsigned Utils::get_instruction_index(llvm::Instruction* I)
{
    unsigned idx = 0;
    auto it = I->getParent()->begin();
    while (&*it != I && it != I->getParent()->end()) {
        ++it;
        ++idx;
    }
    return idx;
}

}


