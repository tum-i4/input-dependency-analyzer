#pragma once

#include <memory>
#include <unordered_map>

#include "PDGLLVMNode.h"

namespace llvm {

class Module;
class Function;
} // namespace llvm

namespace pdg {

class FunctionPDG;

/// Program Dependence Graph
class PDG
{
public:
    using PDGGlobalNodeTy = std::shared_ptr<PDGLLVMGlobalVariableNode>;
    using GlobalVariableNodes = std::unordered_map<llvm::GlobalVariable*, PDGGlobalNodeTy>;
    using FunctionPDGTy = std::shared_ptr<FunctionPDG>;
    using FunctionPDGs = std::unordered_map<llvm::Function*, FunctionPDGTy>;

public:
    explicit PDG(llvm::Module* M)
        : m_module(M)
    {
    }
    
    ~PDG() = default;
    PDG(const PDG& ) = delete;
    PDG(PDG&& ) = delete;
    PDG& operator =(const PDG& ) = delete;
    PDG& operator =(PDG&& ) = delete;
    
public:
    const llvm::Module* getModule() const
    {
        return m_module;
    }

    const GlobalVariableNodes& getGlobalVariableNodes() const
    {
        return m_globalVariableNodes;
    }

    GlobalVariableNodes& getGlobalVariableNodes()
    {
        return m_globalVariableNodes;
    }

    const FunctionPDGs& getFunctionPDGs() const
    {
        return m_functionPDGs;
    }

    FunctionPDGs& getFunctionPDGs()
    {
        return m_functionPDGs;
    }

    bool hasGlobalVariableNode(llvm::GlobalVariable* variable) const
    {
        return m_globalVariableNodes.find(variable) != m_globalVariableNodes.end();
    }

    bool hasFunctionPDG(llvm::Function* F) const
    {
        return m_functionPDGs.find(F) != m_functionPDGs.end();
    }

    PDGGlobalNodeTy getGlobalVariableNode(llvm::GlobalVariable* variable)
    {
        assert(hasGlobalVariableNode(variable));
        return m_globalVariableNodes.find(variable)->second;
    }
    const PDGGlobalNodeTy getGlobalVariableNode(llvm::GlobalVariable* variable) const
    {
        return const_cast<PDG*>(this)->getGlobalVariableNode(variable);
    }

    FunctionPDGTy getFunctionPDG(llvm::Function* F)
    {
        assert(hasFunctionPDG(F));
        return m_functionPDGs.find(F)->second;
    }
    const FunctionPDGTy getFunctionPDG(llvm::Function* F) const
    {
        return const_cast<PDG*>(this)->getFunctionPDG(F);
    }

    bool addGlobalVariableNode(llvm::GlobalVariable* variable, PDGGlobalNodeTy node)
    {
        return m_globalVariableNodes.insert(std::make_pair(variable, node)).second;
    }

    bool addGlobalVariableNode(llvm::GlobalVariable* variable)
    {
        if (hasGlobalVariableNode(variable)) {
            return false;
        }
        m_globalVariableNodes.insert(std::make_pair(variable, PDGGlobalNodeTy(new PDGLLVMGlobalVariableNode(variable))));
        return true;
    }

    bool addFunctionPDG(llvm::Function* F, FunctionPDGTy functionPDG)
    {
        return m_functionPDGs.insert(std::make_pair(F, functionPDG)).second;
    }

private:
    llvm::Module* m_module;
    GlobalVariableNodes m_globalVariableNodes;
    FunctionPDGs m_functionPDGs;
};

} // namespace pdg

