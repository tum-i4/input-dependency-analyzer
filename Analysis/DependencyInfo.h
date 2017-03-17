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
        INPUT_DEP,
        VALUE_DEP
    };


public:
    DepInfo(Dependency dep = UNKNOWN) 
        : dependency(dep)
        , intermediateDependency(UNKNOWN)
    {
    }

    DepInfo(Dependency dep, ArgumentSet&& args)
        : dependency(dep)
        , intermediateDependency(UNKNOWN)
        , argumentDependencies(std::move(args))
    {
    }

    DepInfo(Dependency dep, const ArgumentSet& args)
        : dependency(dep)
        , intermediateDependency(UNKNOWN)
        , argumentDependencies(args)
    {
    }

    DepInfo(Dependency dep, ValueSet&& values)
        : dependency(dep)
        , intermediateDependency(UNKNOWN)
        , valueDependencies(std::move(values))
    {
    }

    DepInfo(Dependency dep, const ValueSet& values)
        : dependency(dep)
        , intermediateDependency(UNKNOWN)
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

    bool isInputDep() const
    {
        return dependency == INPUT_DEP;
    }

    bool isValueDep() const
    {
        return dependency == VALUE_DEP;
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

    void setDependency(Dependency dep)
    {
        dependency = dep;
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

    void collectIntermediateDependency(Dependency dep)
    {
        intermediateDependency = std::max(intermediateDependency, dep);
    }

    void appyIntermediateDependency()
    {
        dependency = intermediateDependency;
        intermediateDependency = UNKNOWN;
    }

private:
    Dependency dependency;
    Dependency intermediateDependency;
    ArgumentSet argumentDependencies;
    ValueSet valueDependencies;
};

}

