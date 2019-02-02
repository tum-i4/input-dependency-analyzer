#include "analysis/InputIndependencyReachabilityAnalysis.h"

#include "PDG/InputDependencyNode.h"
#include "PDG/LLVMNode.h"
#include "PDG/PDG/PDG.h"
#include "PDG/PDG/FunctionPDG.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace input_dependency {

namespace {

void setInputDependency(LLVMNode* node)
{
    assert(node);
    if (!node->getInputDepInfo().isInputDep()) {
        node->setDFInputDepInfo(InputDepInfo(InputDepInfo::INPUT_INDEP));
    }
}

} // unnamed namespace


InputIndependencyReachabilityAnalysis::InputIndependencyReachabilityAnalysis(pdg::PDG* pdg)
    : m_pdg(pdg)
{
}

void InputIndependencyReachabilityAnalysis::analyze()
{
    for (auto f_it = m_pdg->begin(); f_it != m_pdg->end(); ++f_it) {
        llvm::Function* F = (*f_it).first;
        auto fNode = m_pdg->getFunctionNode(F);
        if (auto* llvmFNode = llvm::dyn_cast<LLVMNode>(fNode.get())) {
            setInputDependency(llvmFNode);
        }
        for (auto it = (*f_it).second->nodesBegin();
                it != (*f_it).second->nodesEnd(); ++it) {
            setInputDependency(llvm::dyn_cast<LLVMNode>(*it));
        }
    }
}

} // namespace input_dependency

