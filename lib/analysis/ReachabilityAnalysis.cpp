#include "analysis/ReachabilityAnalysis.h"

#include "PDG/InputDependencyNode.h"
#include "PDG/LLVMNode.h"
#include "PDG/PDG/PDG.h"
#include "PDG/PDG/PDGNode.h"
#include "PDG/PDG/PDGEdge.h"
#include "PDG/PDG/PDGLLVMNode.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"

namespace input_dependency {

ReachabilityAnalysis::ReachabilityAnalysis()
    : m_nodeProcessor([] (NodeType ) {})
{
}

void ReachabilityAnalysis::setNodeProcessor(const NodeProcessor& nodeProcessor)
{
    m_nodeProcessor = nodeProcessor;
}

void ReachabilityAnalysis::analyze(NodeType node,
                                   const ReachCallback& callback)
{
    for (auto out_it = node->outEdgesBegin();
         out_it != node->outEdgesEnd();
         ++out_it) {
        callback(node, (*out_it)->getDestination());
        m_nodeProcessor((*out_it)->getDestination());

        if (llvm::dyn_cast<pdg::PDGLLVMFunctionNode>(node.get())) {
            continue;
        }

        analyze((*out_it)->getDestination(), callback);
    }
}

void ReachabilityAnalysis::propagateDependencies(ReachabilityAnalysis::NodeType node1,
                           ReachabilityAnalysis::NodeType node2)
{
    auto* llvmNode1 = llvm::dyn_cast<LLVMNode>(node1.get());
    auto* llvmNode2 = llvm::dyn_cast<LLVMNode>(node2.get());
    //llvm::dbgs() << "source " << *llvmNode1->getNodeValue() << "\n";
    //llvm::dbgs() << "sink " << *llvmNode2->getNodeValue() << "\n";
    //llvm::dbgs() << "input dep info " << llvmNode1->getInputDepInfo().getDependencyName() << "\n";
    llvmNode2->mergeInputDepInfo(llvmNode1->getInputDepInfo());
}


} // namespace input_dependency

