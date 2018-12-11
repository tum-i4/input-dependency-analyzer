#include "analysis/ReachabilityAnalysis.h"

#include "PDG/InputDependencyNode.h"
#include "PDG/LLVMNode.h"
#include "PDG/PDG/PDG.h"
#include "PDG/PDG/PDGNode.h"
#include "PDG/PDG/PDGEdge.h"
#include "PDG/PDG/PDGLLVMNode.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"

namespace input_dependency {

ReachabilityAnalysis::ReachabilityAnalysis(const GraphType& graph)
    : m_graph(graph)
{
}

void ReachabilityAnalysis::analyze(NodeType node, const ReachCallback& callback)
{
    for (auto out_it = node->outEdgesBegin();
         out_it != node->outEdgesEnd();
         ++out_it) {
        callback(node, (*out_it)->getDestination());
        analyze((*out_it)->getDestination(), callback);
    }
}

} // namespace input_dependency

