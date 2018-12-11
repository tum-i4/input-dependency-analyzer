#include "analysis/InputDependencySources.h"

#include "PDG/PDG/PDG.h"
#include "PDG/PDG/FunctionPDG.h"

#include "PDG/LLVMNode.h"


namespace input_dependency {

InputDependencySources::InputDependencySources(const pdg::PDG& pdg)
    : m_pdg(pdg)
{
}

void InputDependencySources::computeInputSources()
{
    // TODO:
}

} // namespace input_dependency

