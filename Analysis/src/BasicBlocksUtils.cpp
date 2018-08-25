#include "input-dependency/Analysis/BasicBlocksUtils.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"

namespace input_dependency {

void BasicBlocksUtils::addUnreachableBlock(llvm::BasicBlock* block)
{
    m_unreachableBlocks.insert(block);
}

bool BasicBlocksUtils::isBlockUnreachable(llvm::BasicBlock* block) const
{
    return m_unreachableBlocks.find(block) != m_unreachableBlocks.end();
}

long unsigned BasicBlocksUtils::getFunctionUnreachableBlocksCount(llvm::Function* F) const
{
    long unsigned count = 0;
    for (auto& B : *F) {
        if (isBlockUnreachable(&B)) {
            ++count;
        }
    }
    return count;
}

long unsigned BasicBlocksUtils::getFunctionUnreachableInstructionsCount(llvm::Function* F) const
{
    long unsigned count = 0;
    for (auto& B : *F) {
        if (isBlockUnreachable(&B)) {
            count += B.getInstList().size();
        }
    }
    return count;
}

}

