#include "utils/Utils.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <cxxabi.h>

namespace input_dependency {

bool Utils::isLibraryFunction(llvm::Function* F, llvm::Module* M)
{
    assert(F != nullptr);
    assert(M != nullptr);
    return (F->getParent() != M
            || F->isDeclaration());

    //|| F->getLinkage() == llvm::GlobalValue::LinkOnceODRLinkage);
}

std::string Utils::demangle_name(const std::string& name)
{
    int status = -1;
    char* demangled = abi::__cxa_demangle(name.c_str(), NULL, NULL, &status);
    if (status == 0) {
        return std::string(demangled);
    }
    return std::string();
}

unsigned Utils::getFunctionInstrsCount(llvm::Function& F)
{
    unsigned count = 0;
    for (auto& B : F) {
        count += B.getInstList().size();
    }
    return count;
}


}

