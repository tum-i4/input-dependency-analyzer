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

private:
   InputDepInfo m_inputDepInfo; 
};

} // namespace input_dependency

