#include "input-dependency/Analysis/LibraryInfoCollector.h"

namespace input_dependency {

LibraryInfoCollector::LibraryInfoCollector(const LibraryInfoCallback& callback)
    : m_libFunctionInfoProcessor(callback)
{
}

void LibraryInfoCollector::addInputIndepArg(int index, LibArgumentDependenciesMap& argDepMap)
{
    argDepMap.emplace(index, LibArgDepInfo{input_dependency::DepInfo::INPUT_INDEP});
}

void LibraryInfoCollector::addArgWithDeps(int index,
                                          std::unordered_set<int>&& deps,
                                          LibArgumentDependenciesMap& argDepMap)
{
    argDepMap.emplace(index, LibArgDepInfo{input_dependency::DepInfo::INPUT_ARGDEP, std::move(deps)});
}

}

