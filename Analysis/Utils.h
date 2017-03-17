#pragma once

#include "definitions.h"
#include "DependencyAnaliser.h"

namespace input_dependency {

class Utils {

public:
    static bool haveIntersection(const DependencyAnaliser::ArgumentDependenciesMap& inputNums,
                                 const ArgumentSet& selfNums);

}; // class Utils

} // namespace input_dependency

