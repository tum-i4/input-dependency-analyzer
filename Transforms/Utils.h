#pragma once

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"

#include <unordered_set>
#include <vector>

namespace llvm {
class Module;
}

namespace oh {

class Utils {
public:
    static llvm::BasicBlock::iterator get_instruction_pos(llvm::Instruction* I);
    static llvm::Function::iterator get_block_pos(llvm::BasicBlock* block);
    static unsigned get_instruction_index(const llvm::Instruction* I);
    static std::unordered_set<llvm::BasicBlock*> get_blocks_in_range(llvm::Function::iterator begin, llvm::Function::iterator end);
    static std::vector<llvm::BasicBlock*> get_blocks_in_bfs(llvm::Function::iterator begin, llvm::Function::iterator end);
    static unsigned get_function_instrs_count(llvm::Function& F);

    static void check_module(const llvm::Module& M);
};

}

