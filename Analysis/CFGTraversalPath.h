#pragma once

#include <list>
#include <unordered_map>

namespace llvm {
class BasicBlock;
class Loop;
class LoopInfo;
class Function;
}

namespace input_dependency {

class CFGTraversalPathCreator
{
public:
    enum mode {
        SCC,
        CFG
    };
public:
    using BlocksInTraversalOrder = std::list<std::pair<llvm::BasicBlock*, llvm::Loop*>>;
    using BlockToLoopMap =  std::unordered_map<llvm::BasicBlock*, llvm::BasicBlock*>;

public:
    CFGTraversalPathCreator(llvm::Function* F, llvm::LoopInfo* LI);

    const BlocksInTraversalOrder& getBlocksInOrder() const;
    BlocksInTraversalOrder& getBlocksInOrder();
    const BlockToLoopMap& getBlocksLoops() const;
    BlockToLoopMap& getBlocksLoops();

    void construct(mode in_mode);
private:
    void construct_with_scc();
    void construct_with_cfg();

private:
    llvm::Function* m_F;
    llvm::LoopInfo* m_LI;
    BlocksInTraversalOrder m_blockOrder;
    BlockToLoopMap m_loopBlocks;

};

} // namespace input_dependency

