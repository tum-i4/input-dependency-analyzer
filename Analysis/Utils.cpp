#include "Utils.h"

#include "llvm/IR/Argument.h"

namespace input_dependency {

bool Utils::haveIntersection(const DependencyAnaliser::ArgumentDependenciesMap& inputNums,
                             const ArgumentSet& selfNums)
{
    for (auto& self : selfNums) {
        auto pos = inputNums.find(self);
        if (pos == inputNums.end()) {
            return false;
        }
        return pos->second.isInputDep();
    }
    return false;
}

}

