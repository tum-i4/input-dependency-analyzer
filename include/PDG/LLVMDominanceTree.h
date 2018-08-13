#pragma once

#include "PDG/DominanceResults.h"
#include <functional>

namespace llvm {
class DominatorTree;
class PostDominatorTree;
class Function;
}

namespace pdg {

class LLVMDominanceTree : public DominanceResults
{
public:
    using DominatorTreeGetter =
                            std::function<const llvm::DominatorTree* (llvm::Function* F)>;
    using PostDominatorTreeGetter =
                            std::function<const llvm::PostDominatorTree* (llvm::Function* F)>;

public:
    LLVMDominanceTree(const DominatorTreeGetter& domTreeGetter,
                      const PostDominatorTreeGetter& postdomTreeGetter);

    LLVMDominanceTree(const LLVMDominanceTree& ) = delete;
    LLVMDominanceTree(LLVMDominanceTree&& ) = delete;
    LLVMDominanceTree& operator =(const LLVMDominanceTree& ) = delete;
    LLVMDominanceTree& operator =(LLVMDominanceTree&& ) = delete;

public:
    virtual bool dominates(llvm::BasicBlock* blockA, llvm::BasicBlock* blockB) override;
    virtual bool posdominates(llvm::BasicBlock* blockA, llvm::BasicBlock* blockB) override;

private:
    const DominatorTreeGetter& m_domTreeGetter;
    const PostDominatorTreeGetter& m_posdomTreeGetter;
}; // class LLVMDominanceTree

} // namespace pdg

