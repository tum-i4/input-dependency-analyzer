#pragma once

#include <unordered_set>

namespace llvm {
class BasicBlock;
class Function;
}

namespace input_dependency {

class BasicBlocksUtils
{
public:
    static BasicBlocksUtils& get()
    {
        static BasicBlocksUtils blocksUtils;
        return blocksUtils;
    }

public:
    void addUnreachableBlock(llvm::BasicBlock* block);
    bool isBlockUnreachable(llvm::BasicBlock* block) const;
    long unsigned getFunctionUnreachableBlocksCount(llvm::Function* F) const;
    long unsigned getFunctionUnreachableInstructionsCount(llvm::Function* F) const;

private:
    std::unordered_set<llvm::BasicBlock*> m_unreachableBlocks;
};

}

