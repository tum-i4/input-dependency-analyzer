#pragma once

#include <memory>
#include <unordered_map>

#include "PDGLLVMNode.h"
#include "PDGSVFGNode.h"

namespace pdg {
class FunctionPDG
{
public:
    using ArgNodeTy = std::shared_ptr<PDGLLVMArgumentNode>;
    using PDGNodeTy = std::shared_ptr<PDGNode>;
    using PDGLLVMArgumentNodes = std::unordered_map<llvm::Argument*, ArgNodeTy>;
    using PDGLLVMNodes = std::unordered_map<llvm::Value*, PDGNodeTy>;
    using PDGSVFGNodes = std::unordered_map<SVFGNode*, PDGNodeTy>;
    using arg_iterator = PDGLLVMArgumentNodes::iterator;
    using arg_const_iterator = PDGLLVMArgumentNodes::const_iterator;
    using iterator = PDGLLVMNodes::iterator;
    using const_iterator = PDGLLVMNodes::const_iterator;

public:
    explicit FunctionPDG(llvm::Function* F)
        : m_function(F)
    {
    }

    ~FunctionPDG() = default;
    FunctionPDG(const FunctionPDG& ) = delete;
    FunctionPDG(FunctionPDG&& ) = delete;
    FunctionPDG& operator =(const FunctionPDG& ) = delete;
    FunctionPDG& operator =(FunctionPDG&& ) = delete;


public:
    llvm::Function* getFunction()
    {
        return m_function;
    }

    const llvm::Function* getFunction() const
    {
        return const_cast<FunctionPDG*>(this)->getFunction();
    }

    bool hasFormalArgNode(llvm::Argument* arg) const
    {
        return m_formalArgNodes.find(arg) != m_formalArgNodes.end();
    }
    bool hasNode(llvm::Value* value) const
    {
        return m_functionNodes.find(value) != m_functionNodes.end();
    }
    bool hasNode(SVFGNode* svfgNode) const
    {
        return m_functionSVFGNodes.find(svfgNode) != m_functionSVFGNodes.end();
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

    PDGNodeTy getNode(llvm::Value* val)
    {
        assert(hasNode(val));
        return m_functionNodes.find(val)->second;
    }
    const PDGNodeTy getNode(llvm::Value* val) const
    {
        return const_cast<FunctionPDG*>(this)->getNode(val);
    }
    PDGNodeTy getNode(SVFGNode* node)
    {
        assert(hasNode(node));
        return m_functionSVFGNodes.find(node)->second;
    }
    const PDGNodeTy getNode(SVFGNode* node) const
    {
        return const_cast<FunctionPDG*>(this)->getNode(node);
    }

    bool addFormalArgNode(llvm::Argument* arg, ArgNodeTy argNode)
    {
        return m_formalArgNodes.insert(std::make_pair(arg, argNode)).second;
    }
    bool addFormalArgNode(llvm::Argument* arg)
    {
        if (hasFormalArgNode(arg)) {
            return false;
        }
        m_formalArgNodes.insert(std::make_pair(arg, ArgNodeTy(new PDGLLVMArgumentNode(arg))));
        return true;
    }

    bool addNode(llvm::Value* val, PDGNodeTy node)
    {
        return m_functionNodes.insert(std::make_pair(val, node)).second;
    }
    bool addNode(SVFGNode* node, PDGNodeTy svfgNode)
    {
        return m_functionSVFGNodes.insert(std::make_pair(node, svfgNode)).second;
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
    PDGLLVMNodes m_functionNodes;
    PDGSVFGNodes m_functionSVFGNodes;
}; // class FunctionPDG

} // namespace pdg

