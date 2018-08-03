#include "PDG/LLVMMemorySSADefUseAnalysisResults.h"
#include "analysis/IndirectCallSitesAnalysis.h"

#include "PDG/PDGLLVMNode.h"

#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace pdg {

LLVMMemorySSADefUseAnalysisResults::LLVMMemorySSADefUseAnalysisResults(
                        llvm::MemorySSA* memSSA)
    : m_memorySSA(memSSA)
{
}

// TODO: need to first check if node exists
LLVMMemorySSADefUseAnalysisResults::PDGNodeTy LLVMMemorySSADefUseAnalysisResults::getDefSite(llvm::Value* value)
{
    llvm::Instruction* instr = llvm::dyn_cast<llvm::Instruction>(value);
    if (!instr) {
        return PDGNodeTy();
    }
    llvm::MemoryAccess* memAccess = m_memorySSA->getMemoryAccess(instr);
    if (!instr) {
        return PDGNodeTy();
    }
    auto* memUse = llvm::dyn_cast<llvm::MemoryUse>(memAccess);
    if (!memUse) {
        return PDGNodeTy();
    }
    auto* memDefAccess = memUse->getDefiningAccess();
    if (auto* memDef = llvm::dyn_cast<llvm::MemoryDef>(memDefAccess)) {
        auto* memInst = memDef->getMemoryInst();
        if (!memInst) {
            return PDGNodeTy();
        }
        return PDGNodeTy(new PDGLLVMInstructionNode(memInst));
    } else if (auto* memPhi = llvm::dyn_cast<llvm::MemoryPhi>(memDefAccess)) {
        std::vector<llvm::Value*> values;
        std::vector<llvm::BasicBlock*> blocks;
        getPhiValueAndBlocks(memPhi, values, blocks);
        return PDGNodeTy(new PDGPhiNode(values, blocks));
    }
    assert(false);
    return PDGNodeTy();
}

void LLVMMemorySSADefUseAnalysisResults::getPhiValueAndBlocks(llvm::MemoryPhi* memPhi,
                                                              std::vector<llvm::Value*>& values,
                                                              std::vector<llvm::BasicBlock*>& blocks)
{
    for (unsigned i = 0; i < memPhi->getNumIncomingValues(); ++i) {
        llvm::MemoryAccess* incomAccess = memPhi->getIncomingValue(i);
        if (auto* memDef = llvm::dyn_cast<llvm::MemoryDef>(incomAccess)) {
            if (auto* memInst = memDef->getMemoryInst()) {
                values.push_back(memInst);
                blocks.push_back(memPhi->getIncomingBlock(i));
            } else if (auto* phi = llvm::dyn_cast<llvm::MemoryPhi>(incomAccess)) {
                getPhiValueAndBlocks(phi, values, blocks);
            } else {
                assert(false);
            }
        }
    }
}


} // namespace pdg

