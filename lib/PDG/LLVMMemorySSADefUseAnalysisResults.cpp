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
                        const MemorySSAGetter& mssaGetter)
    : m_memorySSAGetter(mssaGetter)
{
}

llvm::Value* LLVMMemorySSADefUseAnalysisResults::getDefSite(llvm::Value* value)
{
    auto* memDefAccess = getMemoryDefAccess(value);
    if (!memDefAccess) {
        return nullptr;
    }
    if (auto* memDef = llvm::dyn_cast<llvm::MemoryDef>(memDefAccess)) {
        return memDef->getMemoryInst();
    }
    return nullptr;
}

DefUseResults::PDGNodeTy LLVMMemorySSADefUseAnalysisResults::getDefSiteNode(llvm::Value* value)
{
    auto* memDefAccess = getMemoryDefAccess(value);
    if (!memDefAccess) {
        return PDGNodeTy();
    }
    if (auto* memDef = llvm::dyn_cast<llvm::MemoryDef>(memDefAccess)) {
        auto* memInst = memDef->getMemoryInst();
        if (!memInst) {
            return PDGNodeTy();
        }
        return PDGNodeTy(new PDGLLVMInstructionNode(memInst));
    } else if (auto* memPhi = llvm::dyn_cast<llvm::MemoryPhi>(memDefAccess)) {
        auto pos = m_phiNodes.find(memPhi->getID());
        if (pos != m_phiNodes.end()) {
            return pos->second;
        }
        std::vector<llvm::Value*> values;
        std::vector<llvm::BasicBlock*> blocks;
        getPhiValueAndBlocks(memPhi, values, blocks);
        PDGNodeTy phiNode = PDGNodeTy(new PDGPhiNode(values, blocks));
        m_phiNodes.insert(std::make_pair(memPhi->getID(), phiNode));
        return phiNode;
    }
    assert(false);
    return PDGNodeTy();
}

llvm::MemoryAccess* LLVMMemorySSADefUseAnalysisResults::getMemoryDefAccess(llvm::Value* value)
{
    llvm::Instruction* instr = llvm::dyn_cast<llvm::Instruction>(value);
    if (!instr) {
        return nullptr;
    }
    auto* memorySSA = m_memorySSAGetter(instr->getParent()->getParent());
    llvm::MemoryAccess* memAccess = memorySSA->getMemoryAccess(instr);
    if (!memAccess) {
        return nullptr;
    }
    auto* memUse = llvm::dyn_cast<llvm::MemoryUse>(memAccess);
    if (!memUse) {
        return nullptr;
    }
    return memUse->getDefiningAccess();
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

