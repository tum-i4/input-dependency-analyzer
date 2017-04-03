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
        : dependency(dep)
    {
    }

    DepInfo(Dependency dep, ArgumentSet&& args)
        : dependency(dep)
        , argumentDependencies(std::move(args))
    {
    }

    DepInfo(Dependency dep, const ArgumentSet& args)
        : dependency(dep)
        , argumentDependencies(args)
    {
    }

    DepInfo(Dependency dep, ValueSet&& values)
        : dependency(dep)
        , valueDependencies(std::move(values))
    {
    }

    DepInfo(Dependency dep, const ValueSet& values)
        : dependency(dep)
        , valueDependencies(values)
    {
    }

public:
    bool isDefined() const
    {
        return dependency != UNKNOWN;
    }

    bool isInputIndep() const
    {
        return dependency == INPUT_INDEP;
    }

    bool isInputArgumentDep() const
    {
        return dependency == INPUT_ARGDEP;
    }

    bool isInputDep() const
    {
        return dependency == INPUT_DEP;
    }

    bool isValueDep() const
    {
        return dependency == VALUE_DEP || !valueDependencies.empty();
    }

    const Dependency& getDependency() const
    {
        return dependency;
    }

    Dependency& getDependency()
    {
        return dependency;
    }
    
    const ArgumentSet& getArgumentDependencies() const
    {
        return argumentDependencies;
    }

    ArgumentSet& getArgumentDependencies()
    {
        return argumentDependencies;
    }

    void setArgumentDependencies(const ArgumentSet& args)
    {
        argumentDependencies = args;
    }

    const ValueSet& getValueDependencies() const
    {
        return valueDependencies;
    }

    ValueSet& getValueDependencies()
    {
        return valueDependencies;
    }

    void setValueDependencies(const ValueSet& valueDeps)
    {
        valueDependencies = valueDeps;
    }

    void setDependency(Dependency dep)
    {
        dependency = dep;
    }

    // for debugging
    std::string getDependencyName() const
    {
        switch (dependency) {
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
        this->dependency = std::max(this->dependency, info.dependency);
        this->valueDependencies.insert(info.valueDependencies.begin(),
                                       info.valueDependencies.end());
        this->argumentDependencies.insert(info.argumentDependencies.begin(),
                                          info.argumentDependencies.end());
    }

    void mergeDependencies(const ArgumentSet& argDeps)
    {
        this->argumentDependencies.insert(argDeps.begin(), argDeps.end());
    }

    void mergeDependencies(const ValueSet& valueDeps)
    {
        this->valueDependencies.insert(valueDeps.begin(), valueDeps.end());
    }

    void mergeDependency(Dependency dep)
    {
        this->dependency = std::max(this->dependency, dep);
    }

private:
    Dependency dependency;
    ArgumentSet argumentDependencies;
    ValueSet valueDependencies;
};

}

