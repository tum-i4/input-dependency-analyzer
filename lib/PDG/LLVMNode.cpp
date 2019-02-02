#include "PDG/LLVMNode.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace input_dependency {

std::string LLVMPhiNode::getNodeAsString() const
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

std::string LLVMVarArgNode::getNodeAsString() const
{
    std::string str;
    llvm::raw_string_ostream rawstr(str);
    rawstr << "VAArg ";
    rawstr << m_function->getName();
    return rawstr.str();
}

}

