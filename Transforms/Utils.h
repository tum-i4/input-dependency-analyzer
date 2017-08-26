#pragma once

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"

namespace oh {

class Utils {
public:
    static llvm::BasicBlock::iterator get_instruction_pos(llvm::Instruction* I);
    static llvm::Function::iterator get_block_pos(llvm::BasicBlock* block);
    static unsigned get_instruction_index(llvm::Instruction* I);
};

}

