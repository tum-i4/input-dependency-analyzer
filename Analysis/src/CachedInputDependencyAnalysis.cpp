#include "input-dependency/Analysis/CachedInputDependencyAnalysis.h"
#include "input-dependency/Analysis/Utils.h"

#include "input-dependency/Analysis/CachedFunctionAnalysisResult.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace input_dependency {

CachedInputDependencyAnalysis::CachedInputDependencyAnalysis(llvm::Module* M)
    : m_module(M)
{
}

void CachedInputDependencyAnalysis::run()
{
    llvm::dbgs() << "Analyze cached input dependency\n";
    for (auto& F : *m_module) {
        if (Utils::isLibraryFunction(&F, m_module)) {
            continue;
        }
        InputDepResType analiser(new CachedFunctionAnalysisResult(&F));
        auto res = m_functionAnalisers.insert(std::make_pair(&F, analiser));
        assert(res.second);
        res.first->second->analyze();
    }
}

bool CachedInputDependencyAnalysis::isInputDependent(llvm::Function* F, llvm::Instruction* instr) const
{
    auto pos = m_functionAnalisers.find(F);
    if (pos == m_functionAnalisers.end()) {
        return false;
        // or even exception
    }
    return pos->second->isInputDependent(instr);
}

bool CachedInputDependencyAnalysis::isInputDependent(llvm::Instruction* instr) const
{
    auto* F = instr->getParent()->getParent();
    assert(F != nullptr);
    return isInputDependent(F, instr);
}

bool CachedInputDependencyAnalysis::isInputDependent(llvm::BasicBlock* block) const
{
    auto F = block->getParent();
    auto pos = m_functionAnalisers.find(F);
    if (pos == m_functionAnalisers.end()) {
        return false;
    }
    return pos->second->isInputDependentBlock(block);
}

bool CachedInputDependencyAnalysis::isControlDependent(llvm::Instruction* I) const
{
    auto F = I->getParent()->getParent();
    auto pos = m_functionAnalisers.find(F);
    if (pos == m_functionAnalisers.end()) {
        return false;
    }
    return pos->second->isControlDependent(I);
}

bool CachedInputDependencyAnalysis::isDataDependent(llvm::Instruction* I) const
{
    auto F = I->getParent()->getParent();
    auto pos = m_functionAnalisers.find(F);
    if (pos == m_functionAnalisers.end()) {
        return false;
    }
    return pos->second->isDataDependent(I);
}

CachedInputDependencyAnalysis::InputDepResType CachedInputDependencyAnalysis::getAnalysisInfo(llvm::Function* F)
{
    auto pos = m_functionAnalisers.find(F);
    if (pos == m_functionAnalisers.end()) {
        return nullptr;
    }
    return pos->second;
}

const CachedInputDependencyAnalysis::InputDepResType CachedInputDependencyAnalysis::getAnalysisInfo(llvm::Function* F) const
{
    auto pos = m_functionAnalisers.find(F);
    if (pos == m_functionAnalisers.end()) {
        return nullptr;
    }
    return pos->second;
}

bool CachedInputDependencyAnalysis::insertAnalysisInfo(llvm::Function* F, InputDepResType analysis_info)
{
    auto pos = m_functionAnalisers.find(F);
    if (pos != m_functionAnalisers.end()) {
        return false;
    }
    m_functionAnalisers.insert(std::make_pair(F, analysis_info));
    return true;
}

}

