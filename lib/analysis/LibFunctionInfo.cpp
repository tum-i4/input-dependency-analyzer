#include "analysis/LibFunctionInfo.h"

#include "llvm/IR/Argument.h"
#include "llvm/IR/Function.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace input_dependency {

namespace {

using IndexToArgumentMap = LibFunctionInfo::IndexToArgumentMap;
using LibArgDepInfo = LibFunctionInfo::LibArgDepInfo;
using ArgumentSet = LibFunctionInfo::ArgumentSet;

IndexToArgumentMap getFunctionIndexToArgMap(llvm::Function* F)
{
    std::unordered_map<int, llvm::Argument*> indexToArg;
    for (auto& arg : F->args()) {
        indexToArg.emplace(arg.getArgNo(), &arg);
    }
    return indexToArg;
}

ArgumentSet getResolvedArguments(const IndexToArgumentMap& indexToArg,
                                 const LibArgDepInfo& libDepInfo)
{
    ArgumentSet arguments;
    for (const auto& dep : libDepInfo.argumentsIdx) {
        auto pos = indexToArg.find(dep);
        assert(pos != indexToArg.end());
        arguments.emplace(pos->second);
    }
    return arguments;
}

} // unnamed namespace

LibFunctionInfo::LibFunctionInfo(const std::string& name)
    : m_name(name)
    , m_isResolved(false)
{
}

LibFunctionInfo::LibFunctionInfo(const std::string& name,
                                 const LibArgumentDependenciesMap& argumentDeps,
                                 const LibArgDepInfo& retDep)
    : m_name(name)
    , m_isResolved(false)
    , m_argumentDependencies(argumentDeps)
    , m_returnDependency(retDep)
{
}

LibFunctionInfo::LibFunctionInfo(std::string&& name,
                                 LibArgumentDependenciesMap&& argumentDeps,
                                 LibArgDepInfo&& retDep)
    : m_name(std::move(name))
    , m_isResolved(false)
    , m_argumentDependencies(std::move(argumentDeps))
    , m_returnDependency(std::move(retDep))
{
}

void LibFunctionInfo::setArgumentDeps(const LibArgumentDependenciesMap& argumentDeps)
{
    m_argumentDependencies = argumentDeps;
}

void LibFunctionInfo::setReturnDeps(const LibArgDepInfo& retDeps)
{
    m_returnDependency = retDeps;
}

void LibFunctionInfo::setCallbackArgumentIndices(const ArgumentIndices& indices)
{
    m_callbackArgumentIndices = indices;
}

const std::string& LibFunctionInfo::getName() const
{
    return m_name;
}

const bool LibFunctionInfo::isResolved() const
{
    return m_isResolved;
}

const LibFunctionInfo::LibArgumentDependenciesMap&
LibFunctionInfo::getArgumentDependencies() const
{
    return m_argumentDependencies;
}

const LibFunctionInfo::LibArgDepInfo&
LibFunctionInfo::getArgumentDependencies(int index) const
{
    auto pos = m_argumentDependencies.find(index);
    assert(pos != m_argumentDependencies.end());
    return pos->second;
}

const LibFunctionInfo::LibArgDepInfo& LibFunctionInfo::getReturnDependency() const
{
    return m_returnDependency;
}

const bool LibFunctionInfo::hasResolvedArgument(llvm::Argument* arg) const
{
    assert(m_isResolved);
    return m_argumentDependencies.find(arg->getArgNo()) != m_argumentDependencies.end();
}

InputDepInfo LibFunctionInfo::getArgumentDependencyInfo(llvm::Argument* arg) const
{
    assert(m_isResolved);
    auto pos = m_argumentDependencies.find(arg->getArgNo());
    return InputDepInfo(pos->second.arguments);
}
InputDepInfo LibFunctionInfo::getReturnDependencyInfo() const
{
    assert(m_isResolved);
    return InputDepInfo(m_returnDependency.arguments);
}

llvm::Argument* LibFunctionInfo::getArgument(int idx) const
{
    auto pos = m_indexToArg.find(idx);
    assert(pos != m_indexToArg.end());
    return pos->second;
}

bool LibFunctionInfo::isCallbackArgument(int index) const
{
    return m_callbackArgumentIndices.find(index) != m_callbackArgumentIndices.end();
}

bool LibFunctionInfo::isCallbackArgument(llvm::Argument* arg) const
{
    return m_callbackArguments.find(arg) != m_callbackArguments.end();
}

void LibFunctionInfo::resolve(llvm::Function* F)
{
    m_indexToArg = getFunctionIndexToArgMap(F);
    resolveArgumentDependencies();
    resolveReturnDependency();
    resolveCallbackArguments();
    m_isResolved = true;
}

void LibFunctionInfo::resolveArgumentDependencies()
{
    for (const auto& argDeps : m_argumentDependencies) {
        auto argPos = m_indexToArg.find(argDeps.first);
        assert(argPos != m_indexToArg.end());
        auto& resolvedDeps = m_argumentDependencies[argPos->first];
        resolvedDeps.dependency = argDeps.second.dependency;
        ArgumentSet arguments = getResolvedArguments(m_indexToArg, argDeps.second);
        resolvedDeps.arguments = std::move(arguments);
    }
    m_argumentDependencies.clear();
}

void LibFunctionInfo::resolveReturnDependency()
{
    m_returnDependency.dependency = m_returnDependency.dependency;
    ArgumentSet arguments = getResolvedArguments(m_indexToArg, m_returnDependency);
    m_returnDependency.arguments = std::move(arguments);
    m_returnDependency.arguments.clear();
    m_returnDependency.dependency = InputDepInfo::UNKNOWN;
}

void LibFunctionInfo::resolveCallbackArguments()
{
    for (const auto& callbackIndex : m_callbackArgumentIndices) {
        auto argPos = m_indexToArg.find(callbackIndex);
        if (argPos != m_indexToArg.end()) {
            m_callbackArguments.insert(argPos->second);
        }
    }
}

} // namespace input_dependency

