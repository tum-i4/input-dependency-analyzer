#include "analysis/ArgumentReachabilityAnalysis.h"

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

ArgumentReachabilityAnalysis::ArgumentReachabilityAnalysis(FunctionPDGType functionPDG)
    : m_functionPDG(functionPDG)
{
}

void ArgumentReachabilityAnalysis::analyze()
{
    llvm::Function* f = m_functionPDG->getFunction();
    for (auto arg_it = f->arg_begin();
         arg_it != f->arg_end();
         ++arg_it) {
        assert(m_functionPDG->hasFormalArgNode(&*arg_it));
        NodeSet processedNodes;
        auto argNode = m_functionPDG->getFormalArgNode(&*arg_it);
        auto* llvmArgNode = llvm::dyn_cast<LLVMNode>(argNode.get());
        llvmArgNode->setDFInputDepInfo(InputDepInfo({&*arg_it}));
        ReachabilityAnalysis::analyze(argNode, &ReachabilityAnalysis::propagateDependencies, processedNodes);
    }
}

} // namespace input_dependency

