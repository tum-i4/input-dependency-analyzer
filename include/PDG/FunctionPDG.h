#pragma once

#include <memory>
#include <unordered_map>

#include "PDGLLVMNode.h"

namespace pdg {
class FunctionPDG
{
public:
    using ArgNodeTy = std::shared_ptr<PDGLLVMArgumentNode>;
    using LLVMNodeTy = std::shared_ptr<PDGLLVMNode>;
    using PDGLLVMArgumentNodes = std::unordered_map<llvm::Argument*, ArgNodeTy>;
    using PDGNodes = std::unordered_map<llvm::Value*, LLVMNodeTy>;
    using arg_iterator = PDGLLVMArgumentNodes::iterator;
    using arg_const_iterator = PDGLLVMArgumentNodes::const_iterator;
    using iterator = PDGNodes::iterator;
    using const_iterator = PDGNodes::const_iterator;

public:
    explicit FunctionPDG(llvm::Function* F);

    ~FunctionPDG() = default;
    FunctionPDG(const FunctionPDG& ) = delete;
    FunctionPDG(FunctionPDG&& ) = delete;
    FunctionPDG& operator =(const FunctionPDG& ) = delete;
    FunctionPDG& operator =(FunctionPDG&& ) = delete;


public:
    bool hasFormalArgNode(llvm::Argument* arg) const
    {
        return m_formalArgNodes.find(arg) != m_formalArgNodes.end();
    }
    bool hasNode(llvm::Value* value) const
    {
        return m_functionNodes.find(value) != m_functionNodes.end();
    }

    ArgNodeTy getFormalArgNode(llvm::Argument* arg)
    {
        assert(hasFormalArgNode(arg));
        return m_formalArgNodes.find(arg)->second;
    }
    const ArgNodeTy getFormalArgNode(llvm::Argument* arg) const
    {
        return const_cast<FunctionPDG*>(this)->getFormalArgNode(arg);
    }

    LLVMNodeTy getNode(llvm::Value* val)
    {
        assert(hasNode(val));
        return m_functionNodes.find(val)->second;
    }
    const LLVMNodeTy getNode(llvm::Value* val) const
    {
        return const_cast<FunctionPDG*>(this)->getNode(val);
    }

    bool addFormalArgNode(llvm::Argument* arg, ArgNodeTy argNode)
    {
        return m_formalArgNodes.insert(std::make_pair(arg, argNode)).second;
    }
    bool addNode(llvm::Value* val, LLVMNodeTy node)
    {
        return m_functionNodes.insert(std::make_pair(val, node)).second;
    }

public:
    arg_iterator formalArgBegin()
    {
        return m_formalArgNodes.begin();
    }
    arg_iterator formalArgEnd()
    {
        return m_formalArgNodes.end();
    }
    arg_const_iterator formalArgBegin() const
    {
        return m_formalArgNodes.begin();
    }
    arg_const_iterator formalArgEnd() const
    {
        return m_formalArgNodes.end();
    }

    iterator nodesBegin()
    {
        return m_functionNodes.begin();
    }
    iterator nodesEnd()
    {
        return m_functionNodes.end();
    }
    const_iterator nodesBegin() const
    {
        return m_functionNodes.begin();
    }
    const_iterator nodesEnd() const
    {
        return m_functionNodes.end();
    }

private:
    llvm::Function* m_function;
    PDGLLVMArgumentNodes m_formalArgNodes;
    // TODO: formal ins, formal outs? formal vaargs?
    PDGNodes m_functionNodes;
}; // class FunctionPDG

} // namespace pdg

