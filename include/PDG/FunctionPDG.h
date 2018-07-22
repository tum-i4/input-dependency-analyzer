#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "PDGLLVMNode.h"
#include "PDGSVFGNode.h"

namespace pdg {
class FunctionPDG
{
public:
    using ArgNodeTy = std::shared_ptr<PDGLLVMFormalArgumentNode>;
    using PDGNodeTy = std::shared_ptr<PDGNode>;
    using PDGLLVMArgumentNodes = std::unordered_map<llvm::Argument*, ArgNodeTy>;
    using PDGLLVMNodes = std::unordered_map<llvm::Value*, PDGNodeTy>;
    using PDGSVFGNodes = std::unordered_map<SVFGNode*, PDGNodeTy>;
    using PDGNodes = std::vector<PDGNodeTy>;
    using arg_iterator = PDGLLVMArgumentNodes::iterator;
    using arg_const_iterator = PDGLLVMArgumentNodes::const_iterator;
    using llvm_iterator = PDGLLVMNodes::iterator;
    using const_llvm_iterator = PDGLLVMNodes::const_iterator;
    using iterator = PDGNodes::iterator;
    using const_iterator = PDGNodes::const_iterator;

public:
    explicit FunctionPDG(llvm::Function* F)
        : m_function(F)
        , m_functionDefinitionBuilt(false)
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

    void setFunctionDefBuilt(bool built)
    {
        m_functionDefinitionBuilt = built;
    }
    bool isFunctionDefBuilt() const
    {
        return m_functionDefinitionBuilt;
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
        return m_functionLLVMNodes.find(value) != m_functionLLVMNodes.end();
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
        return m_functionLLVMNodes.find(val)->second;
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
        auto res = m_formalArgNodes.insert(std::make_pair(arg, argNode));
        if (res.second) {
            m_functionNodes.push_back(res.first->second);
        }
        return res.second;
    }
    bool addFormalArgNode(llvm::Argument* arg)
    {
        if (hasFormalArgNode(arg)) {
            return false;
        }
        auto res = m_formalArgNodes.insert(std::make_pair(arg, ArgNodeTy(new PDGLLVMFormalArgumentNode(arg))));
        assert(res.second);
        m_functionNodes.push_back(res.first->second);
        return true;
    }

    bool addNode(llvm::Value* val, PDGNodeTy node)
    {
        auto res = m_functionLLVMNodes.insert(std::make_pair(val, node));
        if (res.second) {
            m_functionNodes.push_back(res.first->second);
        }
        return res.second;
    }
    bool addNode(SVFGNode* node, PDGNodeTy svfgNode)
    {
        auto res = m_functionSVFGNodes.insert(std::make_pair(node, svfgNode));
        if (res.second) {
            m_functionNodes.push_back(res.first->second);
        }
        return res.second;
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

    llvm_iterator llvmNodesBegin()
    {
        return m_functionLLVMNodes.begin();
    }
    llvm_iterator llvmNodesEnd()
    {
        return m_functionLLVMNodes.end();
    }
    const_llvm_iterator llvmNodesBegin() const
    {
        return m_functionLLVMNodes.begin();
    }
    const_llvm_iterator llvmNodesEnd() const
    {
        return m_functionLLVMNodes.end();
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

    unsigned size() const
    {
        return m_functionNodes.size();
    }

    const std::string getGraphName() const
    {
        return m_function->getName();
    }

private:
    llvm::Function* m_function;
    bool m_functionDefinitionBuilt;
    PDGLLVMArgumentNodes m_formalArgNodes;
    // TODO: formal ins, formal outs? formal vaargs?
    PDGLLVMNodes m_functionLLVMNodes;
    PDGSVFGNodes m_functionSVFGNodes;
    PDGNodes m_functionNodes;
}; // class FunctionPDG

} // namespace pdg

