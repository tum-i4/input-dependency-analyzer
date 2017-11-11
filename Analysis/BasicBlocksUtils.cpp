#include "BasicBlocksUtils.h"

#include "llvm/IR/BasicBlock.h"

namespace input_dependency {

void BasicBlocksUtils::addUnreachableBlock(llvm::BasicBlock* block)
{
    m_unreachableBlocks.insert(block);
}

bool BasicBlocksUtils::isBlockUnreachable(llvm::BasicBlock* block)
{
    return m_unreachableBlocks.find(block) != m_unreachableBlocks.end();
}


}

