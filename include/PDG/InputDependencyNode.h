#pragma once

#include "analysis/InputDepInfo.h"

namespace input_dependency {

class InputDependencyNode
{
public:
    InputDependencyNode() = default;
    InputDependencyNode(const InputDepInfo& DFinputDepInfo,
                        const InputDepInfo& CFinputDepInfo)
        : m_DFinputDepInfo(DFinputDepInfo)
        , m_CFinputDepInfo(CFinputDepInfo)
    {
    }

    InputDepInfo getInputDepInfo() const
    {
        InputDepInfo inputDepInfo= m_DFinputDepInfo;
        inputDepInfo.mergeDependencies(m_CFinputDepInfo);
        return inputDepInfo;
    }

    const InputDepInfo& getDFInputDepInfo() const
    {
        return m_DFinputDepInfo;
    }

    const InputDepInfo& getCFInputDepInfo() const
    {
        return m_CFinputDepInfo;
    }

    void setCFInputDepInfo(const InputDepInfo& inputDepInfo)
    {
        m_CFinputDepInfo = inputDepInfo;
    }

    void setDFInputDepInfo(const InputDepInfo& inputDepInfo)
    {
        m_DFinputDepInfo = inputDepInfo;
    }

    void mergeCFInputDepInfo(const InputDepInfo& inputDepInfo)
    {
        m_CFinputDepInfo.mergeDependencies(inputDepInfo);
    }

    void mergeDFInputDepInfo(const InputDepInfo& inputDepInfo)
    {
        m_DFinputDepInfo.mergeDependencies(inputDepInfo);
    }

private:
   InputDepInfo m_DFinputDepInfo; 
   InputDepInfo m_CFinputDepInfo; 
};

} // namespace input_dependency

