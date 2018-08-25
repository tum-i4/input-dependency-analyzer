#pragma once

#include <algorithm>
#include "input-dependency/Analysis/definitions.h"
#include "llvm/IR/Value.h"

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

    // TODO: maybe keeping global dependencies separatelly will be more efficient
    bool isOnlyGlobalValueDependent() const
    {
        if (m_valueDependencies.empty()) {
            return false;
        }
        for (const auto& val : m_valueDependencies) {
            if (!llvm::dyn_cast<llvm::GlobalVariable>(val)) {
                return false;
            }
        }
        return true;
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

    void mergeDependencies(DepInfo&& info)
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


private:
    Dependency m_dependency;
    ArgumentSet m_argumentDependencies;
    ValueSet m_valueDependencies;
};

inline bool operator ==(const DepInfo& info1, const DepInfo& info2)
{
    if (info1.getDependency() != info2.getDependency()) {
        return false;
    }
    if (info1.getValueDependencies().size() != info2.getValueDependencies().size()) {
        return false;
    }
    if (info1.getArgumentDependencies().size() != info2.getArgumentDependencies().size()) {
        return false;
    }
    if (!std::all_of(info1.getValueDependencies().begin(), info1.getValueDependencies().end(),
                [&info2] (llvm::Value* value) {
                    return info2.getValueDependencies().find(value) != info2.getValueDependencies().end();
                })) {
        return false;
    }
    return (std::all_of(info1.getArgumentDependencies().begin(), info1.getArgumentDependencies().end(),
                [&info2] (llvm::Argument* arg) {
                    return info2.getArgumentDependencies().find(arg) != info2.getArgumentDependencies().end();
                }));
}

inline bool operator !=(const DepInfo& info1, const DepInfo& info2)
{
    return !(info1 == info2);
}
}

