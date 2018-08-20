#include "PDG/SVFGDefUseAnalysisResults.h"

#include "PDG/PDGLLVMNode.h"

#include "SVF/MSSA/SVFG.h"
#include "SVF/MSSA/SVFGNode.h"

namespace pdg {

namespace {

void printNodeType(SVFGNode* node)
{
    /*Addr, Copy, Gep, Store,
      Load, TPhi, TIntraPhi, TInterPhi,
      MPhi, MIntraPhi, MInterPhi, FRet,
      ARet, AParm, APIN, APOUT,
      FParm, FPIN, FPOUT, NPtr*/
    if (node->getNodeKind() == SVFGNode::Addr) {
        llvm::dbgs() << "Addr\n";
    } else if (node->getNodeKind() == SVFGNode::Copy) {
        llvm::dbgs() << "Copy\n";
    } else if (node->getNodeKind() == SVFGNode::Gep) {
        llvm::dbgs() << "Gep\n";
    } else if (node->getNodeKind() == SVFGNode::Store) {
        llvm::dbgs() << "Store\n";
    } else if (node->getNodeKind() == SVFGNode::Load) {
        llvm::dbgs() << "Load\n";
    } else if (node->getNodeKind() == SVFGNode::TPhi) {
        llvm::dbgs() << "TPhi\n";
    } else if (node->getNodeKind() == SVFGNode::TIntraPhi) {
        llvm::dbgs() << "TIntraPhi\n";
    } else if (node->getNodeKind() == SVFGNode::TInterPhi) {
        llvm::dbgs() << "TInterPhi\n";
    } else if (node->getNodeKind() == SVFGNode::MPhi) {
        llvm::dbgs() << "MPhi\n";
    } else if (node->getNodeKind() == SVFGNode::MIntraPhi) {
        llvm::dbgs() << "MIntraPhi\n";
    } else if (node->getNodeKind() == SVFGNode::MInterPhi) {
        llvm::dbgs() << "MInterPhi\n";
    } else if (node->getNodeKind() == SVFGNode::FRet) {
        llvm::dbgs() << "FRet\n";
    } else if (node->getNodeKind() == SVFGNode::ARet) {
        llvm::dbgs() << "ARet\n";
    } else if (node->getNodeKind() == SVFGNode::AParm) {
        llvm::dbgs() << "AParm\n";
    } else if (node->getNodeKind() == SVFGNode::APIN) {
        llvm::dbgs() << "APIN\n";
    } else if (node->getNodeKind() == SVFGNode::APOUT) {
        llvm::dbgs() << "APOUT\n";
    } else if (node->getNodeKind() == SVFGNode::FParm) {
        llvm::dbgs() << "FParm\n";
    } else if (node->getNodeKind() == SVFGNode::FPIN) {
        llvm::dbgs() << "FPIN\n";
    } else if (node->getNodeKind() == SVFGNode::FPOUT) {
        llvm::dbgs() << "FPOUT\n";
    } else if (node->getNodeKind() == SVFGNode::NPtr) {
        llvm::dbgs() << "NPtr\n";
    } else {
        llvm::dbgs() << "Unknown SVFG Node kind\n";
    }
}

bool hasIncomingEdges(PAGNode* pagNode)
{
    //Addr, Copy, Store, Load, Call, Ret, NormalGep, VariantGep, ThreadFork, ThreadJoin
    if (pagNode->hasIncomingEdges(PAGEdge::Addr)
        || pagNode->hasIncomingEdges(PAGEdge::Copy)
        || pagNode->hasIncomingEdges(PAGEdge::Store)
        || pagNode->hasIncomingEdges(PAGEdge::Load)
        || pagNode->hasIncomingEdges(PAGEdge::Call)
        || pagNode->hasIncomingEdges(PAGEdge::Ret)
        || pagNode->hasIncomingEdges(PAGEdge::NormalGep)
        || pagNode->hasIncomingEdges(PAGEdge::VariantGep)
        || pagNode->hasIncomingEdges(PAGEdge::ThreadFork)
        || pagNode->hasIncomingEdges(PAGEdge::ThreadJoin)) {
        return true;
    }
    return false;
}

void getValuesAndBlocks(MSSADEF* def,
                        std::vector<llvm::Value*>& values,
                        std::vector<llvm::BasicBlock*>& blocks)
{
    if (def->getType() == MSSADEF::CallMSSACHI) {
        // TODO:
    } else if (def->getType() == MSSADEF::StoreMSSACHI) {
        auto* storeChi = llvm::dyn_cast<SVFG::STORECHI>(def);
        values.push_back(const_cast<llvm::Instruction*>(storeChi->getStoreInst()->getInst()));
        blocks.push_back(const_cast<llvm::BasicBlock*>(storeChi->getBasicBlock()));
    } else if (def->getType() == MSSADEF::EntryMSSACHI) {
        // TODO:
    } else if (def->getType() == MSSADEF::SSAPHI) {
        auto* phi = llvm::dyn_cast<MemSSA::PHI>(def);
        for (auto it = phi->opVerBegin(); it != phi->opVerEnd(); ++it) {
            getValuesAndBlocks(it->second->getDef(), values, blocks);
        }
    }
}

template <typename PHIType>
void getValuesAndBlocks(PHIType* node,
                        std::vector<llvm::Value*>& values,
                        std::vector<llvm::BasicBlock*>& blocks)
{
    for (auto it = node->opVerBegin(); it != node->opVerEnd(); ++it) {
        auto* def = it->second->getDef();
        getValuesAndBlocks(def, values, blocks);
    }
}

void getValuesAndBlocks(IntraMSSAPHISVFGNode* svfgNode,
                        std::vector<llvm::Value*>& values,
                        std::vector<llvm::BasicBlock*>& blocks)
{
    getValuesAndBlocks<IntraMSSAPHISVFGNode>(svfgNode, values, blocks);
}

void getValuesAndBlocks(InterMSSAPHISVFGNode* svfgNode,
                        std::vector<llvm::Value*>& values,
                        std::vector<llvm::BasicBlock*>& blocks)
{
    getValuesAndBlocks<InterMSSAPHISVFGNode>(svfgNode, values, blocks);
}

void getValuesAndBlocks(PHISVFGNode* svfgNode,
                        std::vector<llvm::Value*>& values,
                        std::vector<llvm::BasicBlock*>& blocks)
{
    // TODO: can not take basic block from pag node
    //for (auto it = svfgNode->opVerBegin(); it != svfgNode->opVerEnd(); ++it) {
    //    auto* pagNode = it->second;
    //    if (pagNode->hasValue()) {
    //        values.push_back(const_cast<llvm::Value*>(pagNode->getValue()));
    //        blocks.push_back(const_cast<llvm::BasicBlock*>(pagNode->getBB()));
    //    }
    //}
}


void getValuesAndBlocks(SVFGNode* svfgNode,
                        std::vector<llvm::Value*>& values,
                        std::vector<llvm::BasicBlock*>& blocks)
{
    if (auto* stmtNode = llvm::dyn_cast<StmtSVFGNode>(svfgNode)) {
        values.push_back(const_cast<llvm::Instruction*>(stmtNode->getInst()));
        blocks.push_back(const_cast<llvm::BasicBlock*>(svfgNode->getBB()));
    } else if (auto* actualParam = llvm::dyn_cast<ActualParmSVFGNode>(svfgNode)) {
        auto* param = actualParam->getParam();
        if (param->hasValue()) {
            values.push_back(const_cast<llvm::Value*>(param->getValue()));
            blocks.push_back(const_cast<llvm::BasicBlock*>(svfgNode->getBB()));
        }
    } else if (auto* actualRet = llvm::dyn_cast<ActualRetSVFGNode>(svfgNode)) {
        auto* ret = actualRet->getRev();
        if (ret->hasValue()) {
            values.push_back(const_cast<llvm::Value*>(ret->getValue()));
            blocks.push_back(const_cast<llvm::BasicBlock*>(svfgNode->getBB()));
        }
    } else if (auto* formalParam = llvm::dyn_cast<FormalParmSVFGNode>(svfgNode)) {
        auto* param = formalParam->getParam();
        if (param->hasValue()) {
            values.push_back(const_cast<llvm::Value*>(param->getValue()));
            blocks.push_back(const_cast<llvm::BasicBlock*>(svfgNode->getBB()));
        }
    } else  if (auto* formalRet = llvm::dyn_cast<FormalRetSVFGNode>(svfgNode)) {
        auto* ret = formalRet->getRet();
        if (ret->hasValue()) {
            values.push_back(const_cast<llvm::Value*>(ret->getValue()));
            blocks.push_back(const_cast<llvm::BasicBlock*>(svfgNode->getBB()));
        }
    } else if (auto* formalInNode = llvm::dyn_cast<FormalINSVFGNode>(svfgNode)) {
        // TODO:
    } else if (auto* formalOutNode = llvm::dyn_cast<FormalOUTSVFGNode>(svfgNode)) {
        getValuesAndBlocks(formalOutNode->getRetMU()->getVer()->getDef(), values, blocks);
    } else if (auto* actualInNode = llvm::dyn_cast<ActualINSVFGNode>(svfgNode)) {
        // TODO:
    } else if (auto* actualOutNode = llvm::dyn_cast<ActualOUTSVFGNode>(svfgNode)) {
        // TODO:
    } else if (auto* intraMssaPhiNode = llvm::dyn_cast<IntraMSSAPHISVFGNode>(svfgNode)) {
        getValuesAndBlocks(intraMssaPhiNode, values, blocks);
    } else if (auto* interMssaPhiNode = llvm::dyn_cast<InterMSSAPHISVFGNode>(svfgNode)) {
        getValuesAndBlocks(interMssaPhiNode, values, blocks);
    } else if (auto* null = llvm::dyn_cast<NullPtrSVFGNode>(svfgNode)) {
    } else if (auto* phiNode = llvm::dyn_cast<PHISVFGNode>(svfgNode)) {
        getValuesAndBlocks(phiNode, values, blocks);
    }
}

}

SVFGDefUseAnalysisResults::SVFGDefUseAnalysisResults(SVFG* svfg)
    : m_svfg(svfg)
{
}

llvm::Value* SVFGDefUseAnalysisResults::getDefSite(llvm::Value* value)
{
    SVFGNode* svfgNode = getSVFGNode(value);
    if (!svfgNode) {
        return nullptr;
    }
    const auto& defNodes = getSVFGDefNodes(svfgNode);
    if (defNodes.size() != 1) {
        return nullptr;
    }
    return getSVFGNodeValue(*defNodes.begin());
}

DefUseResults::PDGNodeTy SVFGDefUseAnalysisResults::getDefSiteNode(llvm::Value* value)
{
    SVFGNode* svfgNode = getSVFGNode(value);
    if (!svfgNode) {
        return PDGNodeTy();
    }
    const auto& defNodes = getSVFGDefNodes(svfgNode);
    return getNode(defNodes);
}

SVFGNode* SVFGDefUseAnalysisResults::getSVFGNode(llvm::Value* value)
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
    if (!hasIncomingEdges(pagNode)) {
        return nullptr;
    }
    if (!m_svfg->hasSVFGNode(pagNode->getId())) {
        return nullptr;
    }
    return const_cast<SVFGNode*>(m_svfg->getDefSVFGNode(pagNode));
}

std::unordered_set<SVFGNode*> SVFGDefUseAnalysisResults::getSVFGDefNodes(SVFGNode* svfgNode)
{
    std::unordered_set<SVFGNode*> defNodes;
    for (auto inedge_it = svfgNode->InEdgeBegin(); inedge_it != svfgNode->InEdgeEnd(); ++inedge_it) {
        SVFGNode* srcNode = (*inedge_it)->getSrcNode();
        if (srcNode->getNodeKind() == SVFGNode::Copy
            || srcNode->getNodeKind() == SVFGNode::Store
            || srcNode->getNodeKind() == SVFGNode::MPhi
            || srcNode->getNodeKind() == SVFGNode::TPhi) {
            defNodes.insert(srcNode);
            // TODO: what other node kinds can be added here?
        } else {
            printNodeType(srcNode);
        }
    }
    return defNodes;
}

llvm::Value* SVFGDefUseAnalysisResults::getSVFGNodeValue(SVFGNode* svfgNode)
{
    if (auto* stmtNode = llvm::dyn_cast<StmtSVFGNode>(svfgNode)) {
        return const_cast<llvm::Instruction*>(stmtNode->getInst());
    }
    return nullptr;
}

DefUseResults::PDGNodeTy SVFGDefUseAnalysisResults::getNode(const std::unordered_set<SVFGNode*>& svfgNodes)
{
    if (svfgNodes.size() == 1) {
        return getNode(*svfgNodes.begin());
    }
    std::vector<llvm::Value*> values;
    std::vector<llvm::BasicBlock*> blocks;
    for (const auto& svfgNode : svfgNodes) {
        getValuesAndBlocks(svfgNode, values, blocks);
    }
    
    return PDGNodeTy();
}

DefUseResults::PDGNodeTy SVFGDefUseAnalysisResults::getNode(SVFGNode* svfgNode)
{
    if (auto* stmtNode = llvm::dyn_cast<StmtSVFGNode>(svfgNode)) {
        llvm::Instruction* instr = const_cast<llvm::Instruction*>(stmtNode->getInst());
        return PDGNodeTy(new PDGLLVMInstructionNode(instr));
    }
    return PDGNodeTy();
    /*
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
    auto pos = m_phiNodes.find(svfgNode->getId());
    if (pos != m_phiNodes.end()) {
        return pos->second;
    }

    // TODO: think about children of PHISVFGNode. They may need to be processed separately.
    if (auto* phi = llvm::dyn_cast<PHISVFGNode>(svfgNode)) {
        auto phiNode = getNode(phi);
        m_phiNodes.insert(std::make_pair(svfgNode->getId(), phiNode));
        return phiNode;
    }
    if (auto* mssaPhi = llvm::dyn_cast<MSSAPHISVFGNode>(svfgNode)) {
        auto phiNode = getNode(mssaPhi);
        m_phiNodes.insert(std::make_pair(svfgNode->getId(), phiNode));
        return phiNode;
    }
    assert(false);
    return PDGNodeTy();
    */
}

} // namespace pdg

