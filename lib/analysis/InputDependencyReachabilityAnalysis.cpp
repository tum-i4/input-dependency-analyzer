#include "analysis/InputDependencyReachabilityAnalysis.h"
#include "analysis/InputDependencySources.h"

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
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"

#include <functional>

namespace input_dependency {

InputDependencyReachabilityAnalysis::InputDependencyReachabilityAnalysis(PDGType pdg)
    : m_pdg(pdg)
{
}

void InputDependencyReachabilityAnalysis::analyze()
{
    InputDependencySources inputDepSources(*m_pdg);
    inputDepSources.computeInputSources();
    const auto& inputSources = inputDepSources.getInputSources();
    for (const auto& source : inputSources) {
        NodeSet processedNodes;
        auto* llvmNode = llvm::dyn_cast<LLVMNode>(source.get());
        llvmNode->setDFInputDepInfo(InputDepInfo(InputDepInfo::INPUT_DEP));
        ReachabilityAnalysis::analyze(source, &ReachabilityAnalysis::propagateDependencies, processedNodes);
    }
}

} // namespace input_dependency

