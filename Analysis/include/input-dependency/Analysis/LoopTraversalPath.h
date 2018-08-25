#pragma once

#include <list>
#include <unordered_set>

namespace llvm {
class BasicBlock;
class Loop;
class LoopInfo;
}

namespace input_dependency {

class LoopTraversalPathCreator
{
public:
    using LoopPathType = std::list<llvm::BasicBlock*>;

public:
    LoopTraversalPathCreator(llvm::LoopInfo& LI,
                             llvm::Loop& L);

public:
    LoopPathType& getPath()
    {
        return m_path;
    }
public:
    void construct();

private:
    bool add_predecessors(llvm::BasicBlock* block, LoopPathType& blocks);
    void add_successors(llvm::BasicBlock* block,
                        const std::unordered_set<llvm::BasicBlock*>& seen_blocks,
                        LoopPathType& blocks);
    void add_to_path(llvm::BasicBlock* block);

private:
    llvm::LoopInfo& m_LI;
    llvm::Loop& m_L;
    std::unordered_set<llvm::BasicBlock*> m_uniquify_map;
    LoopPathType m_path;
};

} // namespace input_dependency
