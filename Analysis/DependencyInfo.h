#pragma once

#include "definitions.h"

namespace input_dependency {

class DepInfo
{
public:
    // order is important
    enum Dependency {
        UNKNOWN,
        INPUT_INDEP,
        VALUE_DEP,
        INPUT_ARGDEP,
        INPUT_DEP
    };

public:
    DepInfo(Dependency dep = UNKNOWN) 
        : m_dependency(dep)
    {
    }

    DepInfo(Dependency dep, ArgumentSet&& args)
        : m_dependency(dep)
        , m_argumentDependencies(std::move(args))
    {
    }

    DepInfo(Dependency dep, const ArgumentSet& args)
        : m_dependency(dep)
        , m_argumentDependencies(args)
    {
    }

    DepInfo(Dependency dep, ValueSet&& values)
        : m_dependency(dep)
        , m_valueDependencies(std::move(values))
    {
    }

    DepInfo(Dependency dep, const ValueSet& values)
        : m_dependency(dep)
        , m_valueDependencies(values)
    {
    }

public:
    bool isDefined() const
    {
        return m_dependency != UNKNOWN;
    }

    bool isInputIndep() const
    {
        return m_dependency == INPUT_INDEP;
    }

    bool isInputArgumentDep() const
    {
        return m_dependency == INPUT_ARGDEP;
    }

    bool isInputDep() const
    {
        return m_dependency == INPUT_DEP;
    }

    bool isValueDep() const
    {
        return m_dependency == VALUE_DEP || !m_valueDependencies.empty();
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
        return m_argumentDependencies;
    }

    ArgumentSet& getArgumentDependencies()
    {
        return m_argumentDependencies;
    }

    void setArgumentDependencies(const ArgumentSet& args)
    {
        m_argumentDependencies = args;
    }

    const ValueSet& getValueDependencies() const
    {
        return m_valueDependencies;
    }

    ValueSet& getValueDependencies()
    {
        return m_valueDependencies;
    }

    void setValueDependencies(const ValueSet& valueDeps)
    {
        m_valueDependencies = valueDeps;
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
        case VALUE_DEP:
            return "value dependent";
        case INPUT_ARGDEP:
            return "input argument dependent";
        case INPUT_DEP:
            return "input dependent";
        }
    }
public:
    void mergeDependencies(const DepInfo& info)
    {
        this->m_dependency = std::max(this->m_dependency, info.m_dependency);
        this->m_valueDependencies.insert(info.m_valueDependencies.begin(),
                                       info.m_valueDependencies.end());
        this->m_argumentDependencies.insert(info.m_argumentDependencies.begin(),
                                          info.m_argumentDependencies.end());
    }

    void mergeDependencies(const ArgumentSet& argDeps)
    {
        this->m_argumentDependencies.insert(argDeps.begin(), argDeps.end());
    }

    void mergeDependencies(const ValueSet& valueDeps)
    {
        this->m_valueDependencies.insert(valueDeps.begin(), valueDeps.end());
    }

    void mergeDependency(Dependency dep)
    {
        this->m_dependency = std::max(this->m_dependency, dep);
    }

    void addOnDepInfo(const DepInfo& info)
    {
        mergeDependency(info.getDependency());
        addOnArgumentDependencies(info.getArgumentDependencies());
        addOnValueDependencies(info.getValueDependencies());
    }

    void addOnDepInfo(DepInfo&& info)
    {
        mergeDependency(info.getDependency());
        addOnArgumentDependencies(info.getArgumentDependencies());
        addOnValueDependencies(info.getValueDependencies());
    }

    void addOnArgumentDependencies(const ArgumentSet& argDeps)
    {
        m_argumentDependencies.insert(argDeps.begin(), argDeps.end());
    }

    void addOnArgumentDependencies(ArgumentSet&& argDeps)
    {
        m_argumentDependencies.insert(argDeps.begin(), argDeps.end());
    }

    void addOnValueDependencies(const ValueSet& valueDeps)
    {
        m_valueDependencies.insert(valueDeps.begin(), valueDeps.end());
    }

    void addOnValueDependencies(ValueSet&& valueDeps)
    {
        m_valueDependencies.insert(valueDeps.begin(), valueDeps.end());
    }

private:
    Dependency m_dependency;
    ArgumentSet m_argumentDependencies;
    ValueSet m_valueDependencies;
};

}

