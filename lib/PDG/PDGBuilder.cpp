#include "PDG/PDGBuilder.h"

#include "PDG/PDGEdge.h"

#include "SVF/MSSA/SVFG.h"
#include "SVF/MSSA/SVFGNode.h"

#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace pdg {

PDGBuilder::PDGBuilder(llvm::Module* M,
                       SVFG* svfg,
                       PointerAnalysis* pta,
                       const FunctionMemSSAGetter& ssaGetter)
    : m_module(M)
    , m_svfg(svfg)
    , m_pta(pta)
    , m_memSSAGetter(ssaGetter)
{
}

void PDGBuilder::build()
{
    m_pdg.reset(new PDG(m_module));
    visitGlobals();

    for (auto& F : *m_module) {
        m_memorySSA = m_memSSAGetter(&F);
        buildFunctionPDG(&F);
        m_currentFPDG.reset();
    }
}

void PDGBuilder::visitGlobals()
{
    for (auto glob_it = m_module->global_begin();
            glob_it != m_module->global_end();
            ++glob_it) {
        m_pdg->addGlobalVariableNode(&*glob_it);
    }
}

PDGBuilder::FunctionPDGTy PDGBuilder::buildFunctionDefinition(llvm::Function* F)
{
    FunctionPDGTy functionPDG = FunctionPDGTy(new FunctionPDG(F));
    m_pdg->addFunctionPDG(F, functionPDG);
    visitFormalArguments(functionPDG, F);
}

void PDGBuilder::buildFunctionPDG(llvm::Function* F)
{
    if (!m_pdg->hasFunctionPDG(F)) {
        m_currentFPDG.reset(new FunctionPDG(F));
        m_pdg->addFunctionPDG(F, m_currentFPDG);
    } else {
        m_currentFPDG = m_pdg->getFunctionPDG(F);
    }
    if (!m_currentFPDG->isFunctionDefBuilt()) {
        visitFormalArguments(m_currentFPDG, F);
    }
    for (auto& B : *F) {
        visitBlock(B);
        visitBlockInstructions(B);
    }
}

void PDGBuilder::visitFormalArguments(FunctionPDGTy functionPDG, llvm::Function* F)
{
    for (auto arg_it = F->arg_begin();
            arg_it != F->arg_end();
            ++arg_it) {
        functionPDG->addFormalArgNode(&*arg_it);
    }
    functionPDG->setFunctionDefBuilt(true);
}

void PDGBuilder::visitBlock(llvm::BasicBlock& B)
{
    m_currentFPDG->addNode(llvm::dyn_cast<llvm::Value>(&B),
            PDGNodeTy(new PDGLLVMBasicBlockNode(&B)));
}

void PDGBuilder::visitBlockInstructions(llvm::BasicBlock& B)
{
    for (auto& I : B) {
        visit(I);
    }
}

void PDGBuilder::visitBranchInst(llvm::BranchInst& I)
{
    // TODO: output this for debug mode only
    llvm::dbgs() << "Branch Inst: " << I << "\n";
    visitTerminatorInst(I);
}

void PDGBuilder::visitLoadInst(llvm::LoadInst& I)
{
    // TODO: output this for debug mode only
    llvm::dbgs() << "Load Inst: " << I << "\n";
    auto destNode = PDGNodeTy(new PDGLLVMInstructionNode(&I));
    m_currentFPDG->addNode(&I, destNode);
    if (auto sourceNode = processSVFGDef(I)) {
        addDataEdge(sourceNode, destNode);
        return;
    }
    if (auto sourceNode = processLLVMSSADef(I)) {
        addDataEdge(sourceNode, destNode);
        return;
    }
}

void PDGBuilder::visitStoreInst(llvm::StoreInst& I)
{
    // TODO: output this for debug mode only
    llvm::dbgs() << "Store Inst: " << I << "\n";
    auto* valueOp = I.getValueOperand();
    auto sourceNode = getNodeFor(valueOp);
    if (!sourceNode) {
        return;
    }
    auto destNode = PDGNodeTy(new PDGLLVMInstructionNode(&I));
    addDataEdge(sourceNode, destNode);
    m_currentFPDG->addNode(&I, destNode);
}

void PDGBuilder::visitGetElementPtrInst(llvm::GetElementPtrInst& I)
{
    llvm::dbgs() << "GetElementPtr Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitPhiNode(llvm::PHINode& I)
{
    llvm::dbgs() << "Phi Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitMemSetInst(llvm::MemSetInst& I)
{
    llvm::dbgs() << "MemSet Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitMemCpyInst(llvm::MemCpyInst& I)
{
    llvm::dbgs() << "MemCpy Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitMemMoveInst(llvm::MemMoveInst &I)
{
    llvm::dbgs() << "MemMove Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitMemTransferInst(llvm::MemTransferInst &I)
{
    llvm::dbgs() << "MemTransfer Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitMemIntrinsic(llvm::MemIntrinsic &I)
{
    llvm::dbgs() << "MemInstrinsic Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitCallInst(llvm::CallInst& I)
{
    // TODO: think about external calls
    llvm::CallSite callSite(&I);
    visitCallSite(callSite);
}

void PDGBuilder::visitInvokeInst(llvm::InvokeInst& I)
{
    llvm::CallSite callSite(&I);
    visitCallSite(callSite);
    visitTerminatorInst(I);
}

void PDGBuilder::visitTerminatorInst(llvm::TerminatorInst& I)
{
    auto sourceNode = getInstructionNodeFor(&I);
    for (unsigned i = 0; i < I.getNumSuccessors(); ++i) {
        auto* block = I.getSuccessor(i);
        auto destNode = getNodeFor(block);
        addControlEdge(sourceNode, destNode);
    }
}

void PDGBuilder::visitInstruction(llvm::Instruction& I)
{
    auto destNode = getInstructionNodeFor(&I);
    for (auto op_it = I.op_begin(); op_it != I.op_end(); ++op_it) {
        auto sourceNode = getNodeFor(op_it->get());
        addDataEdge(sourceNode, destNode);
    }
}

void PDGBuilder::visitCallSite(llvm::CallSite& callSite)
{
    auto destNode = getInstructionNodeFor(callSite.getInstruction());
    const auto& callees = getCallees(callSite);
    for (unsigned i = 0; i < callSite.getNumArgOperands(); ++i) {
        if (auto* val = llvm::dyn_cast<llvm::Value>(callSite.getArgOperand(i))) {
            auto sourceNode = getNodeFor(val);
            addDataEdge(sourceNode, destNode);
            auto actualArgNode = PDGNodeTy(new PDGLLVMActualArgumentNode(callSite, val));
            // connect actual args with formal args
            addActualArgumentNodeConnections(actualArgNode, i, callees);
        }
    }
}

void PDGBuilder::addDataEdge(PDGNodeTy source, PDGNodeTy dest)
{
    PDGNode::PDGEdgeType edge = PDGNode::PDGEdgeType(new PDGDataEdge(source, dest));
    source->addOutEdge(edge);
    dest->addInEdge(edge);
}

void PDGBuilder::addControlEdge(PDGNodeTy source, PDGNodeTy dest)
{
    PDGNode::PDGEdgeType edge = PDGNode::PDGEdgeType(new PDGControlEdge(source, dest));
    source->addOutEdge(edge);
    dest->addInEdge(edge);
}

PDGBuilder::PDGNodeTy PDGBuilder::processLLVMSSADef(llvm::Instruction& I)
{
    // connect I to its defs using llvm MemorySSA
    llvm::MemoryAccess* memAccess = m_memorySSA->getMemoryAccess(&I);
    if (!memAccess) {
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
        return getInstructionNodeFor(memInst);
    } else if (auto* memPhi = llvm::dyn_cast<llvm::MemoryPhi>(memDefAccess)) {
        return getNodeFor(memPhi);
    }
    assert(false);
    return PDGNodeTy();
}

PDGBuilder::PDGNodeTy PDGBuilder::processSVFGDef(llvm::Instruction& I)
{
    auto* pag = m_svfg->getPAG();
    if (!pag->hasValueNode(&I)) {
        return PDGNodeTy();
    }
    auto nodeId = pag->getValueNode(&I);
    auto* pagNode = pag->getPAGNode(nodeId);
    if (!pagNode) {
        return PDGNodeTy();
    }
    if (!m_svfg->hasSVFGNode(pagNode->getId())) {
        return PDGNodeTy();
    }
    const SVFGNode* defNode = m_svfg->getDefSVFGNode(pagNode);
    if (!defNode) {
        return PDGNodeTy();
    }
    auto sourceNode = getNodeFor(const_cast<SVFGNode*>(defNode));
    return sourceNode;
}

PDGBuilder::PDGNodeTy PDGBuilder::getInstructionNodeFor(llvm::Instruction* instr)
{
    if (m_currentFPDG->hasNode(instr)) {
        return m_currentFPDG->getNode(instr);
    }
    m_currentFPDG->addNode(instr, PDGNodeTy(new PDGLLVMInstructionNode(instr)));
    return m_currentFPDG->getNode(instr);
}

PDGBuilder::PDGNodeTy PDGBuilder::getNodeFor(llvm::Value* value)
{
    if (m_currentFPDG->hasNode(value)) {
        return m_currentFPDG->getNode(value);
    }
    if (auto* global = llvm::dyn_cast<llvm::GlobalVariable>(value)) {
        if (!m_pdg->hasGlobalVariableNode(global)) {
            m_pdg->addGlobalVariableNode(global);
        }
        return m_pdg->getGlobalVariableNode(global);
    }
    if (auto* argument = llvm::dyn_cast<llvm::Argument>(value)) {
        assert(m_currentFPDG->hasFormalArgNode(argument));
        return m_currentFPDG->getFormalArgNode(argument);
    }

    if (auto* constant = llvm::dyn_cast<llvm::Constant>(value)) {
        m_currentFPDG->addNode(value, PDGNodeTy(new PDGLLVMConstantNode(constant)));
    } else if (auto* instr = llvm::dyn_cast<llvm::Instruction>(value)) {
        m_currentFPDG->addNode(value, PDGNodeTy(new PDGLLVMInstructionNode(instr)));
    } else {
        // do not assert here for now to keep track of possible values to be handled here
        llvm::dbgs() << "Unhandled value " << *value << "\n";
        return PDGNodeTy();
    }
    return m_currentFPDG->getNode(value);
}

PDGBuilder::PDGNodeTy PDGBuilder::getNodeFor(llvm::BasicBlock* block)
{
    if (!m_currentFPDG->hasNode(block)) {
        m_currentFPDG->addNode(block, PDGNodeTy(new PDGLLVMBasicBlockNode(block)));
    }
    return m_currentFPDG->getNode(block);
}

PDGBuilder::PDGNodeTy PDGBuilder::getNodeFor(llvm::MemoryPhi* memPhi)
{
    auto memPhiNode = PDGNodeTy(new PDGLLVMemoryAccessNode(memPhi));
    for (int i = 0; i < memPhi->getNumIncomingValues(); ++i) {
        llvm::MemoryAccess* incomingVal = memPhi->getIncomingValue(i);
        PDGNodeTy incomNode;
        if (auto* def = llvm::dyn_cast<llvm::MemoryDef>(incomingVal)) {
            incomNode = getInstructionNodeFor(def->getMemoryInst());
        } else if (auto* phi = llvm::dyn_cast<llvm::MemoryPhi>(incomingVal)) {
            incomNode = getNodeFor(phi);
        }
        if (incomNode) {
            addDataEdge(incomNode, memPhiNode);
        }
    }
    return memPhiNode;
}

PDGBuilder::PDGNodeTy PDGBuilder::getNodeFor(SVFGNode* svfgNode)
{
    if (auto* stmtNode = llvm::dyn_cast<StmtSVFGNode>(svfgNode)) {
        llvm::Instruction* instr = const_cast<llvm::Instruction*>(stmtNode->getInst());
        return getInstructionNodeFor(instr);
    }
    if (auto* actualParam = llvm::dyn_cast<ActualParmSVFGNode>(svfgNode)) {
        // TODO:
        assert(false);
        return PDGNodeTy();
    }
    if (auto* actualRet = llvm::dyn_cast<ActualRetSVFGNode>(svfgNode)) {
        // TODO:
        assert(false);
        return PDGNodeTy();
    }
    if (auto* formalParam = llvm::dyn_cast<FormalParmSVFGNode>(svfgNode)) {
        // TODO:
        assert(false);
        return PDGNodeTy();
    }
    if (auto* formalRet = llvm::dyn_cast<FormalRetSVFGNode>(svfgNode)) {
        // TODO:
        assert(false);
        return PDGNodeTy();
    }
    if (auto* null = llvm::dyn_cast<NullPtrSVFGNode>(svfgNode)) {
        return PDGNodeTy(new PDGNullNode());
    }
    // TODO: think about children of PHISVFGNode. They may need to be processed separately.
    if (auto* phi = llvm::dyn_cast<PHISVFGNode>(svfgNode)) {
        auto phiNode = PDGNodeTy(new PDGPHISVFGNode(phi));
        for (auto it = phi->opVerBegin(); it != phi->opVerEnd(); ++it) {
            auto* pagNode = it->second;
            if (!pagNode) {
                continue;
            }
            if (!m_svfg->hasSVFGNode(pagNode->getId())) {
                continue;
            }
            auto* sn = m_svfg->getSVFGNode(pagNode->getId());
            auto sourceNode = getNodeFor(sn);
            m_currentFPDG->addNode(sn, sourceNode);
            addDataEdge(sourceNode, phiNode);
        }
        return phiNode;
    }
    if (auto* mssaPhi = llvm::dyn_cast<MSSAPHISVFGNode>(svfgNode)) {
        auto mssaPhiNode = PDGNodeTy(new PDGMSSAPHISVFGNode(mssaPhi));
        for (auto it = mssaPhi->opVerBegin(); it != mssaPhi->opVerEnd(); ++it) {
            const MSSADEF* mdef = const_cast<MSSADEF*>(it->second->getDef());
            if (const MemSSA::CALLCHI* callChi = llvm::dyn_cast<const MemSSA::CALLCHI>(mdef)) {
                //TODO:
                assert(false);
            } else if (const MemSSA::ENTRYCHI* mssaEntryChi = llvm::dyn_cast<const MemSSA::ENTRYCHI>(mdef)) {
                //TODO:
                assert(false);
            } else if (const MemSSA::STORECHI* mssaStoreChi = llvm::dyn_cast<const MemSSA::STORECHI>(mdef)) {
                PDGNodeTy sourceNode = getInstructionNodeFor(const_cast<llvm::Instruction*>(mssaStoreChi->getStoreInst()->getInst()));
                addDataEdge(sourceNode, mssaPhiNode);
            } else if (const MemSSA::PHI* phiChi = llvm::dyn_cast<MemSSA::PHI>(mdef)) {
                //TODO: recurse??
                assert(false);
            }
        }
        return mssaPhiNode;
    }
    assert(false);
    return PDGNodeTy();
}

PDGBuilder::FunctionSet PDGBuilder::getCallees(llvm::CallSite& callSite) const
{
    if (!callSite.isIndirectCall()) {
        return FunctionSet{callSite.getCalledFunction()};
    }
    FunctionSet callees;
    auto* ptaCallGraph = m_pta->getPTACallGraph();
    if (ptaCallGraph->hasIndCSCallees(callSite)) {
        const auto& ptaCallees = ptaCallGraph->getIndCSCallees(callSite);
        for (auto& F : ptaCallees) {
            callees.insert(const_cast<llvm::Function*>(F));
        }
    }
    return callees;
}

void PDGBuilder::addActualArgumentNodeConnections(PDGNodeTy actualArgNode,
                                                  unsigned argIdx,
                                                  const FunctionSet& callees)
{
    for (auto& F : callees) {
        if (!m_pdg->hasFunctionPDG(F)) {
            buildFunctionDefinition(F);
        }
        FunctionPDGTy calleePDG = m_pdg->getFunctionPDG(F);
        // TODO: consider varargs
        llvm::Argument* formalArg = &*(F->arg_begin() + argIdx);
        auto formalArgNode = calleePDG->getFormalArgNode(formalArg);
        addDataEdge(actualArgNode, formalArgNode);
    }
}

} // namespace pdg

