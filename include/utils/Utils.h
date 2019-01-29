#pragma once

#include <string>

namespace llvm {
class Function;
class Module;
}

namespace input_dependency {

class Utils {

public:
    static bool isLibraryFunction(llvm::Function* F, llvm::Module* M);
    static std::string demangle_name(const std::string& name);
}; // class Utils

} // namespace input_dependency

