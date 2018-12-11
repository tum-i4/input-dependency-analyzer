#include "analysis/ArgumentReachabilityAnalysis.h"
#include "analysis/ReachabilityAnalysis.h"

#include "PDG/InputDependencyNode.h"
#include "PDG/LLVMNode.h"
#include "PDG/PDG/PDG.h"
#include "PDG/PDG/FunctionPDG.h"
#include "PDG/PDG/PDGNode.h"
#include "PDG/PDG/PDGEdge.h"
#include "PDG/PDG/PDGLLVMNode.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"

#include <functional>

namespace input_dependency {


void propagateDependencies(ReachabilityAnalysis::NodeType node1,
                           ReachabilityAnalysis::NodeType node2)
{
    auto* llvmNode1 = llvm::dyn_cast<LLVMNode>(node1.get());
    auto* llvmNode2 = llvm::dyn_cast<LLVMNode>(node2.get());
    llvmNode1->mergeInputDepInfo(llvmNode2->getInputDepInfo());
}

ArgumentReachabilityAnalysis::ArgumentReachabilityAnalysis(FunctionPDGType functionPDG)
    : m_functionPDG(functionPDG)
{
}

void ArgumentReachabilityAnalysis::analyze()
{
    ReachabilityAnalysis ra;
    llvm::Function* f = m_functionPDG->getFunction();
    for (auto arg_it = f->arg_begin();
         arg_it != f->arg_end();
         ++arg_it) {
        assert(m_functionPDG->hasFormalArgNode(&*arg_it));
        auto argNode = m_functionPDG->getFormalArgNode(&*arg_it);
        auto* llvmArgNode = llvm::dyn_cast<LLVMNode>(argNode.get());
        llvmArgNode->setInputDepInfo(InputDepInfo({&*arg_it}));
        ra.analyze(argNode, propagateDependencies);
    }
}

} // namespace input_dependency

