#pragma once

#include "DependencyAnalysisResult.h"
#include "DependencyAnaliser.h"

namespace input_dependency {

/**
* \class ReflectingDependencyAnaliser
* \brief Interface to reflect dependency info in a unit of analysis
**/
class ReflectingDependencyAnaliser : public virtual DependencyAnalysisResult
{
public:
    ReflectingDependencyAnaliser() = default;
    ReflectingDependencyAnaliser(const ReflectingDependencyAnaliser&) = delete;
    ReflectingDependencyAnaliser(ReflectingDependencyAnaliser&& ) = delete;
    ReflectingDependencyAnaliser& operator =(const ReflectingDependencyAnaliser&) = delete;
    ReflectingDependencyAnaliser& operator =(ReflectingDependencyAnaliser&&) = delete;

    virtual ~ReflectingDependencyAnaliser() = default;

    /// \name Abstract interface of reflection
    /// \{
public:
    virtual void reflect(const DependencyAnaliser::ValueDependencies& dependencies) = 0;
    virtual bool isReflected() const = 0;
    /// \}

}; // class ReflectingDependencyAnaliser

} // namespace input_dependency
