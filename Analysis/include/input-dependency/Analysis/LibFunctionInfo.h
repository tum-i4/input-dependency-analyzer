#pragma once

#include "input-dependency/Analysis/ValueDepInfo.h"

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
    using ArgumentDependenciesMap = std::unordered_map<llvm::Argument*, ValueDepInfo>;
    using IndexToArgumentMap = std::unordered_map<int, llvm::Argument*>;
    using ArgumentIndices = std::unordered_set<int>;

public:
    LibFunctionInfo(const std::string& name);
    LibFunctionInfo(const std::string& name,
                    const LibArgumentDependenciesMap& argumentDeps,
                    const LibArgDepInfo& retDep);
    LibFunctionInfo(std::string&& name,
                    LibArgumentDependenciesMap&& argumentDeps,
                    LibArgDepInfo&& retDep);

public:
    void setArgumentDeps(const LibArgumentDependenciesMap& argumentDeps);
    void setReturnDeps(const LibArgDepInfo& retDeps);
    void setCallbackArgumentIndices(const ArgumentIndices& indices);
    const std::string& getName() const;
    const bool isResolved() const;

    const LibArgumentDependenciesMap& getArgumentDependencies() const;
    const LibArgDepInfo& getArgumentDependencies(int index) const;
    const LibArgDepInfo& getReturnDependency() const;
    const ArgumentDependenciesMap& getResolvedArgumentDependencies() const;
    const bool hasResolvedArgument(llvm::Argument* arg) const;
    const ValueDepInfo& getResolvedArgumentDependencies(llvm::Argument* arg) const;
    const ValueDepInfo& getResolvedReturnDependency() const;

    bool isCallbackArgument(int index) const;
    bool isCallbackArgument(llvm::Argument* arg) const;

public:
    void resolve(llvm::Function* F);

private:
    void resolveArgumentDependencies(const IndexToArgumentMap& indexToArg);
    void resolveReturnDependency(const IndexToArgumentMap& indexToArg);
    void resolveCallbackArguments(const IndexToArgumentMap& indexToArg);


private:
    const std::string m_name;
    bool m_isResolved;
    LibArgumentDependenciesMap m_argumentDependencies;
    LibArgDepInfo m_returnDependency;
    ArgumentDependenciesMap m_resolvedArgumentDependencies;
    ValueDepInfo m_resolvedReturnDependency;
    ArgumentIndices m_callbackArgumentIndices;
    ArgumentSet m_callbackArguments;
}; // class LibFunctionInfo

} // namespace input_dependency

