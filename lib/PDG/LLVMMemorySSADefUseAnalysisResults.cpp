#include "PDG/LLVMMemorySSADefUseAnalysisResults.h"
#include "analysis/IndirectCallSitesAnalysis.h"

#include "PDG/PDGLLVMNode.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace pdg {

LLVMMemorySSADefUseAnalysisResults::LLVMMemorySSADefUseAnalysisResults(
                        const MemorySSAGetter& mssaGetter,
                        const AARGetter& aarGetter)
    : m_memorySSAGetter(mssaGetter)
    , m_aarGetter(aarGetter)
{
}

llvm::Value* LLVMMemorySSADefUseAnalysisResults::getDefSite(llvm::Value* value)
{
    llvm::Instruction* instr = llvm::dyn_cast<llvm::Instruction>(value);
    if (!instr) {
        return nullptr;
    }
    auto* memorySSA = m_memorySSAGetter(instr->getParent()->getParent());
    auto* memDefAccess = getMemoryDefAccess(instr, memorySSA);
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
    llvm::Instruction* instr = llvm::dyn_cast<llvm::Instruction>(value);
    if (!instr) {
        return PDGNodeTy();
    }
    auto* memorySSA = m_memorySSAGetter(instr->getParent()->getParent());
    llvm::Function* F = instr->getFunction();
    auto* aa = m_aarGetter(F);

    auto* memDefAccess = getMemoryDefAccess(instr, memorySSA);
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
        const auto& defSites = getDefSites(value, memPhi, memorySSA, aa);
        PDGNodeTy phiNode = PDGNodeTy(new PDGPhiNode(defSites.values, defSites.blocks));
        return phiNode;
    }
    assert(false);
    return PDGNodeTy();
}

llvm::MemoryAccess* LLVMMemorySSADefUseAnalysisResults::getMemoryDefAccess(llvm::Instruction* instr,
                                                                           llvm::MemorySSA* memorySSA)
{
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

LLVMMemorySSADefUseAnalysisResults::PHI
LLVMMemorySSADefUseAnalysisResults::getDefSites(llvm::Value* value,
                                                llvm::MemoryAccess* access,
                                                llvm::MemorySSA* memorySSA,
                                                llvm::AAResults* aa)
{
    PHI phi;
    if (!access || !value) {
        return phi;
    }
    if (memorySSA->isLiveOnEntryDef(access)) {
        return phi;
    }
    if (auto* def = llvm::dyn_cast<llvm::MemoryDef>(access)) {
        const auto& DL = def->getBlock()->getModule()->getDataLayout();
        llvm::ModRefInfo modRef;
        if (auto* load = llvm::dyn_cast<llvm::LoadInst>(value)) {
            modRef = aa->getModRefInfo(def->getMemoryInst(),
                                      load->getPointerOperand(),
                                      DL.getTypeStoreSize(load->getType()));
        } else if (value->getType()->isSized()) {
            modRef = aa->getModRefInfo(def->getMemoryInst(), value,
                                       DL.getTypeStoreSize(value->getType()));
        } else {
            return phi;
        }
        if (modRef == llvm::ModRefInfo::MustMod || modRef == llvm::ModRefInfo::Mod) {
            phi.values.push_back(def->getMemoryInst());
            phi.blocks.push_back(def->getBlock());
            return phi;
        }
        return getDefSites(value, def->getDefiningAccess(), memorySSA, aa);
    }
    if (auto* memphi = llvm::dyn_cast<llvm::MemoryPhi>(access)) {
        for (auto def_it = memphi->defs_begin(); def_it != memphi->defs_end(); ++def_it) {
            auto accesses = getDefSites(value, *def_it, memorySSA, aa);
            if (!accesses.empty()) {
                phi.values.insert(phi.values.end(), accesses.values.begin(), accesses.values.end());
                phi.blocks.insert(phi.blocks.end(), accesses.blocks.begin(), accesses.blocks.end());
            }
        }
    }
    return phi;
}

} // namespace pdg

