#pragma once

#include <algorithm>
#include <unordered_set>

#include "llvm/IR/Value.h"

namespace llvm {
class Argument;
class GlobalVariable;
} // namespace llvm

namespace input_dependency {

class InputDepInfo
{
public:
    // order is important
    enum Dependency {
        UNKNOWN,
        INPUT_INDEP,
        ARG_DEP,
        GLOBAL_DEP,
        INPUT_DEP
    };

    using ArgumentSet = std::unordered_set<llvm::Argument*>;
    using GlobalsSet = std::unordered_set<llvm::GlobalVariable*>;


public:
    InputDepInfo()
        : m_dependency(UNKNOWN)
    {
    }

    InputDepInfo(Dependency dep)
        : m_dependency(dep)
    {
    }

    InputDepInfo(const ArgumentSet& arguments)
        : m_dependency(ARG_DEP)
        , m_arguments(arguments)
    {
    }

    InputDepInfo(const GlobalsSet& globals)
        : m_dependency(GLOBAL_DEP)
        , m_globals(globals)
    {
    }

    InputDepInfo(ArgumentSet&& arguments)
        : m_dependency(ARG_DEP)
        , m_arguments(std::move(arguments))
    {
    }

    InputDepInfo(GlobalsSet&& globals)
        : m_dependency(GLOBAL_DEP)
        , m_globals(std::move(globals))
    {
    }

    InputDepInfo(const InputDepInfo&) = default;
    InputDepInfo(InputDepInfo&&) = default;
    InputDepInfo& operator =(const InputDepInfo&) = default;
    InputDepInfo& operator =(InputDepInfo&&) = default;

public:
    bool isDefined() const
    {
        return m_dependency != UNKNOWN;
    }

    bool isInputIndep() const
    {
        return m_dependency == INPUT_INDEP;
    }

    bool isArgumentDep() const
    {
        return m_dependency == ARG_DEP || !m_arguments.empty();
    }

    bool isInputDep() const
    {
        return m_dependency == INPUT_DEP;
    }

    bool isGlobalDep() const
    {
        return m_dependency == GLOBAL_DEP;
    }

    const Dependency& getDependency() const
    {
        return m_dependency;
    }

    Dependency& getDependency()
    {
        return m_dependency;
    }
    
    const ArgumentSet& getArgumentDependencies() const
    {
        return m_arguments;
    }

    ArgumentSet& getArgumentDependencies()
    {
        return m_arguments;
    }

    void setArgumentDependencies(const ArgumentSet& args)
    {
        m_arguments = args;
    }

    void setArgumentDependencies(ArgumentSet&& args)
    {
        m_arguments = std::move(args);
    }

    const GlobalsSet& getGlobalDependencies() const
    {
        return m_globals;
    }

    GlobalsSet& getGlobalDependencies()
    {
        return m_globals;
    }

    void setGlobalDependencies(const GlobalsSet& globals)
    {
        m_globals = globals;
    }

    void setGlobalDependencies(GlobalsSet&& globals)
    {
        m_globals = std::move(globals);
    }

    void setDependency(Dependency dep)
    {
        m_dependency = dep;
    }

    // for debugging
    std::string getDependencyName() const
    {
        switch (m_dependency) {
        case UNKNOWN:
            return "unknonw";
        case INPUT_INDEP:
            return "input independent";
        case GLOBAL_DEP:
            return "global dependent";
        case ARG_DEP:
            return "input argument dependent";
        case INPUT_DEP:
            return "input dependent";
        }
    }
public:
    void mergeDependencies(const InputDepInfo& info)
    {
        this->m_dependency = std::max(this->m_dependency, info.m_dependency);
        this->m_globals.insert(info.m_globals.begin(),
                               info.m_globals.end());
        this->m_arguments.insert(info.m_arguments.begin(),
                                 info.m_arguments.end());
    }

    void mergeDependencies(InputDepInfo&& info)
    {
        this->m_dependency = std::max(this->m_dependency, info.m_dependency);
        this->m_globals.insert(info.m_globals.begin(),
                               info.m_globals.end());
        this->m_arguments.insert(info.m_arguments.begin(),
                                 info.m_arguments.end());
    }

    void mergeDependencies(const ArgumentSet& argDeps)
    {
        this->m_arguments.insert(argDeps.begin(), argDeps.end());
    }

    void mergeDependencies(const GlobalsSet& globals)
    {
        this->m_globals.insert(globals.begin(), globals.end());
    }

    void mergeDependency(Dependency dep)
    {
        this->m_dependency = std::max(this->m_dependency, dep);
    }

private:
    Dependency m_dependency;
    ArgumentSet m_arguments;
    GlobalsSet m_globals;
}; // class InputDepInfo

} // namespace input_dependency

