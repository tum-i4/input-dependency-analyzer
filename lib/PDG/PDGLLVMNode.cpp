#include "PDG/PDGLLVMNode.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace pdg {

std::string PDGLLVMNode::getNodeAsString() const
{
    std::string str;
    llvm::raw_string_ostream rawstr(str);
    rawstr << *m_value;
    return rawstr.str();
}

std::string PDGPhiNode::getNodeAsString() const
{
    std::string str;
    llvm::raw_string_ostream rawstr(str);
    rawstr << "phi";
    for (unsigned i = 0; i < m_values.size(); ++i) {
        rawstr << " [ ";
        rawstr << *m_values[i];
        rawstr << m_blocks[i]->getName();
        rawstr << "] ";
    }
    return rawstr.str();
}

} // namespace pdg

