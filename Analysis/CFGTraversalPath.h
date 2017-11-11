#pragma once

#include <list>
#include <unordered_map>
#include <unordered_set>

namespace llvm {
class BasicBlock;
class Loop;
class LoopInfo;
class DominatorTree;
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
    CFGTraversalPathCreator(llvm::Function& F);

    void setLoopInfo(llvm::LoopInfo* LI);
    void setDomTree(const llvm::DominatorTree* domTree);

    const BlocksInTraversalOrder& getBlocksInOrder() const;
    BlocksInTraversalOrder& getBlocksInOrder();
    const BlockToLoopMap& getBlocksLoops() const;
    BlockToLoopMap& getBlocksLoops();

    void construct(mode in_mode);

private:
    void construct_with_scc();
    void construct_with_cfg();
    bool isBlockUnreachable(llvm::BasicBlock* block);

private:
    llvm::Function& m_F;
    llvm::LoopInfo* m_LI;
    const llvm::DominatorTree* m_domTree;
    BlocksInTraversalOrder m_blockOrder;
    BlockToLoopMap m_loopBlocks;
};

} // namespace input_dependency

