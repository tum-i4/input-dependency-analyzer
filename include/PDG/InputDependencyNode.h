#pragma once

#include "analysis/InputDepInfo.h"

namespace input_dependency {

class InputDependencyNode
{
public:
    InputDependencyNode() = default;
    InputDependencyNode(const InputDepInfo& inputDepInfo)
        : m_inputDepInfo(inputDepInfo)
    {
    }

    const InputDepInfo& getInputDepInfo() const
    {
        return m_inputDepInfo;
    }

    void setInputDepInfo(const InputDepInfo& inputDepInfo)
    {
        m_inputDepInfo = inputDepInfo;
    }

    void mergeInputDepInfo(const InputDepInfo& inputDepInfo)
    {
        m_inputDepInfo.mergeDependencies(inputDepInfo);
    }

private:
   InputDepInfo m_inputDepInfo; 
};

} // namespace input_dependency

