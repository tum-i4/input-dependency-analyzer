#pragma once

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"

#include <unordered_set>

namespace oh {

class Utils {
public:
    static llvm::BasicBlock::iterator get_instruction_pos(llvm::Instruction* I);
    static llvm::Function::iterator get_block_pos(llvm::BasicBlock* block);
    static unsigned get_instruction_index(llvm::Instruction* I);
    static std::unordered_set<llvm::BasicBlock*> get_blocks_in_range(llvm::Function::iterator begin, llvm::Function::iterator end);
    static unsigned get_function_instrs_count(llvm::Function& F);
};

}

