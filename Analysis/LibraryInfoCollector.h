#pragma once

#include "LibFunctionInfo.h"

#include <functional>

namespace input_dependency {

class LibraryInfoCollector
{
public:
    using LibraryInfoCallback = std::function<void (LibFunctionInfo&& functionInfo)>;

public:
    LibraryInfoCollector(const LibraryInfoCallback& callback);

    LibraryInfoCollector(const LibraryInfoCollector& ) = delete;
    LibraryInfoCollector(LibraryInfoCollector&& ) = delete;
    LibraryInfoCollector& operator =(const LibraryInfoCollector& ) = delete;
    LibraryInfoCollector& operator =(LibraryInfoCollector&& ) = delete;

public:
    virtual void setup() = 0;

protected:
    using LibArgDepInfo = input_dependency::LibFunctionInfo::LibArgDepInfo;
    using LibArgumentDependenciesMap = input_dependency::LibFunctionInfo::LibArgumentDependenciesMap;

    static void addInputIndepArg(int index, LibArgumentDependenciesMap& argDepMap);
    static void addArgWithDeps(int index,
                               std::unordered_set<int>&& deps,
                               LibArgumentDependenciesMap& argDepMap);

protected:
    const LibraryInfoCallback& m_libFunctionInfoProcessor;
};

} // namespace input_dependency

