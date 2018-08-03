#include "PDG/SVFGDefUseAnalysisResults.h"

#include "PDG/PDGLLVMNode.h"

#include "SVF/MSSA/SVFG.h"
#include "SVF/MSSA/SVFGNode.h"

namespace pdg {

namespace {

void getPhiValueAndBlocks(MSSADEF* mdef,
                          std::vector<llvm::Value*>& values,
                          std::vector<llvm::BasicBlock*>& blocks)
{
    if (const MemSSA::CALLCHI* callChi = llvm::dyn_cast<const MemSSA::CALLCHI>(mdef)) {
        //TODO:
        assert(false);
    } else if (const MemSSA::ENTRYCHI* mssaEntryChi = llvm::dyn_cast<const MemSSA::ENTRYCHI>(mdef)) {
        //TODO:
        assert(false);
    } else if (const MemSSA::STORECHI* mssaStoreChi = llvm::dyn_cast<const MemSSA::STORECHI>(mdef)) {
        values.push_back(const_cast<llvm::Instruction*>(mssaStoreChi->getStoreInst()->getInst()));
        blocks.push_back(const_cast<llvm::BasicBlock*>(mssaStoreChi->getBasicBlock()));
    } else if (const MemSSA::PHI* phiChi = llvm::dyn_cast<MemSSA::PHI>(mdef)) {
        for (unsigned i = 0; i < phiChi->getOpVerNum(); ++i) {
            getPhiValueAndBlocks(phiChi->getOpVer(i)->getDef(), values, blocks);
        }
    }
}

DefUseResults::PDGNodeTy getNode(ActualParmSVFGNode* svfgNode)
{
    // TODO:
    assert(false);
    return DefUseResults::PDGNodeTy();
}

DefUseResults::PDGNodeTy getNode(ActualRetSVFGNode* svfgNode)
{
    // TODO:
    assert(false);
    return DefUseResults::PDGNodeTy();
}

DefUseResults::PDGNodeTy getNode(FormalParmSVFGNode* svfgNode)
{
    // TODO:
    assert(false);
    return DefUseResults::PDGNodeTy();
}

DefUseResults::PDGNodeTy getNode(FormalRetSVFGNode* svfgNode)
{
    // TODO:
    assert(false);
    return DefUseResults::PDGNodeTy();
}

DefUseResults::PDGNodeTy getNode(PHISVFGNode* phiNode)
{
    std::vector<llvm::Value*> values;
    std::vector<llvm::BasicBlock*> blocks;
    for (unsigned i = 0; i < phiNode->getOpVerNum(); ++i) {
        PAGNode* pagNode = const_cast<PAGNode*>(phiNode->getOpVer(i));
        if (pagNode->hasValue()) {
            auto* value = const_cast<llvm::Value*>(pagNode->getValue());
            if (auto* instr = llvm::dyn_cast<llvm::Instruction>(value)) {
                values.push_back(value);
                blocks.push_back(instr->getParent());
            }
        } 
    }
    return DefUseResults::PDGNodeTy(new PDGPhiNode(values, blocks));
}

DefUseResults::PDGNodeTy getNode(MSSAPHISVFGNode* mssaPhi)
{
    std::vector<llvm::Value*> values;
    std::vector<llvm::BasicBlock*> blocks;
    getPhiValueAndBlocks(const_cast<MemSSA::MDEF*>(mssaPhi->getRes()), values, blocks);
    return DefUseResults::PDGNodeTy(new PDGPhiNode(values, blocks));
}

}

SVFGDefUseAnalysisResults::SVFGDefUseAnalysisResults(SVFG* svfg)
    : m_svfg(svfg)
{
}

llvm::Value* SVFGDefUseAnalysisResults::getDefSite(llvm::Value* value)
{
    const SVFGNode* defNode = getDefNode(value);
    if (!defNode) {
        return nullptr;
    }
    if (auto* stmtNode = llvm::dyn_cast<StmtSVFGNode>(defNode)) {
        return const_cast<llvm::Instruction*>(stmtNode->getInst());
    }
    if (auto* actualParam = llvm::dyn_cast<ActualParmSVFGNode>(defNode)) {
        // TODO: to be implemented
        assert(false);
    }
    if (auto* actualRet = llvm::dyn_cast<ActualRetSVFGNode>(defNode)) {
        // TODO: to be implemented
        assert(false);
    }
    if (auto* formalParam = llvm::dyn_cast<FormalParmSVFGNode>(defNode)) {
        // TODO: to be implemented
        assert(false);
    }
    if (auto* formalRet = llvm::dyn_cast<FormalRetSVFGNode>(defNode)) {
        // TODO: to be implemented
        assert(false);
    }
    return nullptr;
}

DefUseResults::PDGNodeTy SVFGDefUseAnalysisResults::getDefSiteNode(llvm::Value* value)
{
    const SVFGNode* defNode = getDefNode(value);
    if (!defNode) {
        return PDGNodeTy();
    }
    return getNode(defNode);
}

SVFGNode* SVFGDefUseAnalysisResults::getDefNode(llvm::Value* value)
{
    llvm::Instruction* instr = llvm::dyn_cast<llvm::Instruction>(value);
    if (!instr) {
        return nullptr;
    }
    auto* pag = m_svfg->getPAG();
    if (!pag->hasValueNode(instr)) {
        return nullptr;
    }
    auto nodeId = pag->getValueNode(instr);
    auto* pagNode = pag->getPAGNode(nodeId);
    if (!pagNode) {
        return nullptr;
    }
    if (!m_svfg->hasSVFGNode(pagNode->getId())) {
        return nullptr;
    }
    return const_cast<SVFGNode*>(m_svfg->getDefSVFGNode(pagNode));
}

DefUseResults::PDGNodeTy SVFGDefUseAnalysisResults::getNode(const SVFGNode* svfgNode)
{
    if (auto* stmtNode = llvm::dyn_cast<StmtSVFGNode>(svfgNode)) {
        llvm::Instruction* instr = const_cast<llvm::Instruction*>(stmtNode->getInst());
        return PDGNodeTy(new PDGLLVMInstructionNode(instr));
    }
    if (auto* actualParam = llvm::dyn_cast<ActualParmSVFGNode>(svfgNode)) {
        return getNode(actualParam);
    }
    if (auto* actualRet = llvm::dyn_cast<ActualRetSVFGNode>(svfgNode)) {
        return getNode(actualRet);
    }
    if (auto* formalParam = llvm::dyn_cast<FormalParmSVFGNode>(svfgNode)) {
        return getNode(formalParam);
    }
    if (auto* formalRet = llvm::dyn_cast<FormalRetSVFGNode>(svfgNode)) {
        return getNode(formalRet);
    }
    if (auto* null = llvm::dyn_cast<NullPtrSVFGNode>(svfgNode)) {
        return PDGNodeTy(new PDGNullNode());
    }
    // TODO: think about children of PHISVFGNode. They may need to be processed separately.
    if (auto* phi = llvm::dyn_cast<PHISVFGNode>(svfgNode)) {
        return getNode(phi);
    }
    if (auto* mssaPhi = llvm::dyn_cast<MSSAPHISVFGNode>(svfgNode)) {
        return getNode(mssaPhi);
    }
    assert(false);
    return PDGNodeTy();
}

} // namespace pdg

