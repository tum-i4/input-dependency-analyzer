#pragma once

namespace llvm {
class BasicBlock;
}

namespace pdg {

/// Interface to query dominance relationship of llvm objects
class DominanceResults
{
public:
    virtual bool dominates(llvm::BasicBlock* blockA, llvm::BasicBlock* blockB) = 0;
    virtual bool posdominates(llvm::BasicBlock* blockA, llvm::BasicBlock* blockB) = 0;
};

} // namespace pdg

