#pragma once

#include "DependencyInfo.h"

namespace llvm {

class Argument;
class Function;

}

namespace input_dependency {

class LibFunctionInfo
{
public:
    struct LibArgDepInfo
    {
        DepInfo::Dependency dependency;
        std::unordered_set<int> argumentDependencies;
    };

public:
    using LibArgumentDependenciesMap = std::unordered_map<int, LibArgDepInfo>;
    using ArgumentDependenciesMap = std::unordered_map<llvm::Argument*, DepInfo>;
    using IndexToArgumentMap = std::unordered_map<int, llvm::Argument*>;

public:
    LibFunctionInfo(const std::string& name,
                    const LibArgumentDependenciesMap& argumentDeps,
                    const LibArgDepInfo& retDep);
    LibFunctionInfo(std::string&& name,
                    LibArgumentDependenciesMap&& argumentDeps,
                    LibArgDepInfo&& retDep);

public:
    const std::string& getName() const;
    const bool isResolved() const;
    const LibArgumentDependenciesMap& getArgumentDependencies() const;
    const LibArgDepInfo& getArgumentDependencies(int index) const;
    const LibArgDepInfo& getReturnDependency() const;
    const ArgumentDependenciesMap& getResolvedArgumentDependencies() const;
    const bool hasResolvedArgument(llvm::Argument* arg) const;
    const DepInfo& getResolvedArgumentDependencies(llvm::Argument* arg) const;
    const DepInfo& getResolvedReturnDependency() const;

public:
    void resolve(llvm::Function* F);

private:
    void resolveArgumentDependencies(const IndexToArgumentMap& indexToArg);
    void resolveReturnDependency(const IndexToArgumentMap& indexToArg);

private:
    const std::string m_name;
    bool m_isResolved;
    LibArgumentDependenciesMap m_argumentDependencies;
    LibArgDepInfo m_returnDependency;
    ArgumentDependenciesMap m_resolvedArgumentDependencies;
    DepInfo m_resolvedReturnDependency;
}; // class LibFunctionInfo

} // namespace input_dependency

