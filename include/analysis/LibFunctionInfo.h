#pragma once

#include "analysis/InputDepInfo.h"

#include <unordered_set>
#include <unordered_map>

namespace llvm {

class Argument;
class Function;

} // namespace llvm

namespace input_dependency {

class LibFunctionInfo
{
public:
    struct LibArgDepInfo
    {
        InputDepInfo::Dependency dependency;
        std::unordered_set<int> argumentsIdx;
        std::unordered_set<llvm::Argument*> arguments;
    };

    using LibArgumentDependenciesMap = std::unordered_map<int, LibArgDepInfo>;
    using IndexToArgumentMap = std::unordered_map<int, llvm::Argument*>;
    using ArgumentIndices = std::unordered_set<int>;
    using ArgumentSet = std::unordered_set<llvm::Argument*>;

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
    const bool hasResolvedArgument(llvm::Argument* arg) const;
    InputDepInfo getArgumentDependencyInfo(llvm::Argument* arg) const;
    InputDepInfo getReturnDependencyInfo() const;

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
    ArgumentIndices m_callbackArgumentIndices;
    ArgumentSet m_callbackArguments;
}; 

} // namespace input_dependency

