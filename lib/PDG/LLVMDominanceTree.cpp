#include "PDG/LLVMDominanceTree.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace pdg {

LLVMDominanceTree::LLVMDominanceTree(const DominatorTreeGetter& domTreeGetter,
                                     const PostDominatorTreeGetter& postdomTreeGetter)
    : m_domTreeGetter(domTreeGetter)
    , m_posdomTreeGetter(postdomTreeGetter)
{
}

bool LLVMDominanceTree::dominates(llvm::BasicBlock* blockA, llvm::BasicBlock* blockB)
{
    auto* domTree = m_domTreeGetter(blockA->getParent());
    return domTree->dominates(blockA, blockB);
}

bool LLVMDominanceTree::posdominates(llvm::BasicBlock* blockA, llvm::BasicBlock* blockB)
{
    auto* postdomTree = m_posdomTreeGetter(blockA->getParent());
    return postdomTree->dominates(blockA, blockB);
}

}

