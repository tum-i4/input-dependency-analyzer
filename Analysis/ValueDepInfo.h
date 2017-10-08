#pragma once

#include "DependencyInfo.h"

#include <vector>
#include <functional>

namespace llvm {
class AllocaInst;
class Instruction;
}

namespace input_dependency {

/**
 * \class ValueDepInfo
 * \brief Represents input dependency information for a value
 * For composite values, such as structs, arrays, etc., has info for each element
 * TODO: see if DepInfo is necessary
 */
class ValueDepInfo
{
public:
    using ValueDeps = std::vector<DepInfo>;

public:
    ValueDepInfo() = default;

    ValueDepInfo(llvm::Value* val);
    ValueDepInfo(llvm::AllocaInst* alloca);
    ValueDepInfo(llvm::Value* val, const DepInfo& depInfo);

public:
    llvm::Value* getValue() const;
    const DepInfo& getValueDep() const;
    DepInfo& getValueDep();
    const ValueDeps& getCompositeValueDeps() const;
    ValueDeps& getCompositeValueDeps();
    const DepInfo& getValueDep(llvm::Instruction* el_instr);

    void updateValueDep(const ValueDepInfo& valueDepInfo);
    void updateValueDep(const DepInfo& depInfo);
    void updateCompositeValueDep(const DepInfo& depInfo);
    void updateValueDep(llvm::Instruction* el_instr,
                        const DepInfo& depInfo);
    void mergeDependencies(const ValueDepInfo& depInfo);

// interface of DepInfo. Eventually DepInfo may be removed altogether
public:
    bool isDefined() const
    {
        return m_depInfo.isDefined();
    }

    bool isInputIndep() const
    {
        return m_depInfo.isInputIndep();
    }

    bool isInputArgumentDep() const
    {
        return m_depInfo.isInputArgumentDep();
    }

    bool isInputDep() const
    {
        return m_depInfo.isInputDep();
    }

    bool isValueDep() const
    {
        return m_depInfo.isValueDep();
    }

    // TODO: maybe keeping global dependencies separatelly will be more efficient
    bool isOnlyGlobalValueDependent() const
    {
        return m_depInfo.isOnlyGlobalValueDependent();
    }

    const DepInfo::Dependency& getDependency() const
    {
        return m_depInfo.getDependency();
    }

    DepInfo::Dependency& getDependency()
    {
        return m_depInfo.getDependency();
    }
    
    const ArgumentSet& getArgumentDependencies() const
    {
        return m_depInfo.getArgumentDependencies();
    }

    ArgumentSet& getArgumentDependencies()
    {
        return m_depInfo.getArgumentDependencies();
    }

    void setArgumentDependencies(const ArgumentSet& args)
    {
        m_depInfo.setArgumentDependencies(args);
    }

    const ValueSet& getValueDependencies() const
    {
        return m_depInfo.getValueDependencies();
    }

    ValueSet& getValueDependencies()
    {
        return m_depInfo.getValueDependencies();
    }

    void setValueDependencies(const ValueSet& valueDeps)
    {
        setValueDependencies(valueDeps);
    }

    void setDependency(DepInfo::Dependency dep)
    {
        m_depInfo.setDependency(dep);
    }

    void mergeDependencies(const ArgumentSet& argDeps)
    {
        m_depInfo.mergeDependencies(argDeps);
    }

    void mergeDependencies(const ValueSet& valueDeps)
    {
        m_depInfo.mergeDependencies(valueDeps);
    }

    void mergeDependency(DepInfo::Dependency dep)
    {
        m_depInfo.mergeDependency(dep);
    }

    void mergeDependencies(const DepInfo& info)
    {
        m_depInfo.mergeDependencies(info);
    }

    void mergeDependencies(DepInfo&& info)
    {
        m_depInfo.mergeDependencies(std::move(info));
    }

private:
    llvm::Value* m_value;
    DepInfo m_depInfo;
    ValueDeps m_elementDeps;
}; // class ValueDepInfo

} // namespace input_dependency

