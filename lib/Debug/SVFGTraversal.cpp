#include "llvm/Pass.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "SVF/MSSA/SVFG.h"
#include "SVF/MSSA/SVFGBuilder.h"
#include "SVF/Util/SVFModule.h"
#include "SVF/MemoryModel/PointerAnalysis.h"
#include "SVF/WPA/Andersen.h"

class SVFGTraversal : public llvm::ModulePass
{
public:
    static char ID;
    SVFGTraversal()
        : llvm::ModulePass(ID)
    {
    }

    bool runOnModule(llvm::Module& M) override
    {
        SVFModule svfM(M);
        AndersenWaveDiff* ander = new AndersenWaveDiff();
        ander->disablePrintStat();
        ander->analyze(svfM);
        SVFGBuilder memSSA(true);
        SVFG *svfg = memSSA.buildSVFG((BVDataPTAImpl*)ander);
        for (auto& F : M) {
            if (F.isDeclaration()) {
                continue;
            }
            for (auto arg_it = F.arg_begin(); arg_it != F.arg_end(); ++arg_it) {
                llvm::dbgs() << "Argument " << *arg_it << "\n";
                process(svfg, &*arg_it);
            }
            for (auto& B : F) {
                for (auto& I : B) {
                    llvm::dbgs() << "Instr: " << I << "\n";
                    process(svfg, &I);
                }
            }
        }
        return false;
    }

    void process(SVFG* svfg, llvm::Value* I)
    {
        auto* pag = svfg->getPAG();
        if (!pag->hasValueNode(I)) {
            llvm::dbgs() << "   No PAG node\n";
            return;
        }
        auto nodeId = pag->getValueNode(I);
        auto* pagNode = pag->getPAGNode(nodeId);
        llvm::dbgs() << "   PAG Node " << *pagNode << "\n";
        if (!svfg->hasSVFGNode(nodeId)) {
            llvm::dbgs() << "   No SVFG node\n";
            return;
        }
        auto* svfgNode = svfg->getSVFGNode(nodeId);
        llvm::dbgs() << "   SVFG node " << *svfgNode << "\n";
        if (auto* stmtNode = llvm::dyn_cast<StmtSVFGNode>(svfgNode)) {
            llvm::dbgs() << "       Stmt Node\n";
            if (auto* pagSrcNode = stmtNode->getPAGSrcNode()) {
                llvm::dbgs() << "       PAG Src Node " << *pagSrcNode << "\n";
            }
            if (auto* pagDstNode = stmtNode->getPAGDstNode()) {
                llvm::dbgs() << "       PAG Dst Node " << *pagDstNode << "\n";
            }
        } else if (auto* actualParamNode = llvm::dyn_cast<ActualParmSVFGNode>(svfgNode)) {
            llvm::dbgs() << "       Actual param node\n";
            llvm::dbgs() << "       Call site " << *(actualParamNode->getCallSite().getInstruction()) << "\n";
            llvm::dbgs() << "       Param " << *actualParamNode->getParam() << "\n";
        } else if (auto* actualRet = llvm::dyn_cast<ActualRetSVFGNode>(svfgNode)) {
            llvm::dbgs() << "       Actual ret node\n";
            llvm::dbgs() << "       Call site " << *(actualRet->getCallSite().getInstruction()) << "\n";
            llvm::dbgs() << "       Rev " << *actualRet->getRev() << "\n";
        } else if (auto* formalParam = llvm::dyn_cast<FormalParmSVFGNode>(svfgNode)) {
            llvm::dbgs() << "       Formal param node\n";
            llvm::dbgs() << "       Function " << formalParam->getFun()->getName() << "\n";
            llvm::dbgs() << "       Param " << *formalParam->getParam() << "\n";
            for (auto it = formalParam->callPEBegin(); it != formalParam->callPEEnd(); ++it) {
                llvm::dbgs() << "       callPE callSite " << *((*it)->getCallInst()) << "\n";
                llvm::dbgs() << "       source node " << *(*it)->getSrcNode() << "\n";
                llvm::dbgs() << "       dest node " << *(*it)->getDstNode() << "\n";
            }
        } else  if (auto* formalRet = llvm::dyn_cast<FormalRetSVFGNode>(svfgNode)) {
            llvm::dbgs() << "       Formal ret node\n";
            llvm::dbgs() << "       Function " << formalRet->getFun()->getName() << "\n";
            llvm::dbgs() << "       Ret " << *formalRet->getRet() << "\n";
            for (auto it = formalRet->retPEBegin(); it != formalRet->retPEEnd(); ++it) {
                llvm::dbgs() << "       retPE callSite " << *(*it)->getCallInst() << "\n";
                llvm::dbgs() << "       source node " << *(*it)->getSrcNode() << "\n";
                llvm::dbgs() << "       dest node " << *(*it)->getDstNode() << "\n";
            }
        } else if (auto* mrsvfgNode = llvm::dyn_cast<MRSVFGNode>(svfgNode)) {
            llvm::dbgs() << "       MRSVFGNode\n";
            assert(false);
        } else if (auto* null = llvm::dyn_cast<NullPtrSVFGNode>(svfgNode)) {
            llvm::dbgs() << "       Null Node\n";
        } else if (auto* phi = llvm::dyn_cast<PHISVFGNode>(svfgNode)) {
            llvm::dbgs() << "       PHI node\n";
            for (auto it = phi->opVerBegin(); it != phi->opVerEnd(); ++it) {
                llvm::dbgs() << "       Op ver " << *it->second << "\n";
            }
        }
        // else if (auto* mssaPhi = llvm::dyn_cast<MSSAPHISVFGNode>(svfgNode)) {
                   // }
    }

};

char SVFGTraversal::ID = 0;
static llvm::RegisterPass<SVFGTraversal> X("svfg-traversal","Traverse SVFG graph and print information");

