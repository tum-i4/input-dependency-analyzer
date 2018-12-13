#pragma once

#include <memory>
#include <vector>

namespace llvm {
class Module;
}

namespace pdg {

class PDG;
class PDGNode;

}

namespace input_dependency {

/// Computes input dependency sources
class InputDependencySources
{
public:
    InputDependencySources(const pdg::PDG& pdg);

    InputDependencySources(const InputDependencySources& ) = delete;
    InputDependencySources(InputDependencySources&& ) = delete;
    InputDependencySources& operator =(const InputDependencySources& ) = delete;
    InputDependencySources& operator =(InputDependencySources&& ) = delete;

public:
    using PDGNodeTy = std::shared_ptr<pdg::PDGNode>;
    using PDGNodes = std::vector<PDGNodeTy>;

    const PDGNodes& getInputSources() const
    {
        return m_inputSources;
    }

    void computeInputSources();

private:
    void addMainArguments();
    void addInputsFromLibraryFunctions();

private:    
    const pdg::PDG& m_pdg;
    const llvm::Module* m_module;
    PDGNodes m_inputSources;
}; // class InputDependencySources

} // namespace input_dependency

