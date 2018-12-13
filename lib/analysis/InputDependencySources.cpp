#include "analysis/InputDependencySources.h"
#include "analysis/LibFunctionInfo.h"
#include "analysis/LibraryInfoManager.h"

#include "PDG/PDG/PDG.h"
#include "PDG/PDG/FunctionPDG.h"

#include "PDG/LLVMNode.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Function.h"

namespace input_dependency {

InputDependencySources::InputDependencySources(const pdg::PDG& pdg)
    : m_pdg(pdg)
    , m_module(pdg.getModule())
{
}

void InputDependencySources::computeInputSources()
{
    addMainArguments();
    addInputsFromLibraryFunctions();
    const auto& libInfoMgr = LibraryInfoManager::get();
}

void InputDependencySources::addMainArguments()
{
    llvm::Function* function = m_module->getFunction("main");
    if (!function) {
        return;
    }
    assert(m_pdg.hasFunctionPDG(function));
    auto functionPDG = m_pdg.getFunctionPDG(function);
    for (auto arg_it = functionPDG->formalArgBegin();
         arg_it != functionPDG->formalArgEnd();
         ++arg_it) {
        m_inputSources.push_back(arg_it->second);
    }
}

void InputDependencySources::addInputsFromLibraryFunctions()
{
    auto& libInfoMgr = LibraryInfoManager::get();
    for (auto& F : *m_module) {
        if (!F.isDeclaration()) {
            continue;
        }
        if (!libInfoMgr.hasLibFunctionInfo(F.getName()))  {
            continue;
        }
        if (!m_pdg.hasFunctionPDG(const_cast<llvm::Function*>(&F))) {
            continue;
        }
        auto fPDG = m_pdg.getFunctionPDG(const_cast<llvm::Function*>(&F));
        // TODO: consider demangling name
        libInfoMgr.resolveLibFunctionInfo(const_cast<llvm::Function*>(&F), F.getName());
        auto& functionLibInfo = libInfoMgr.getLibFunctionInfo(F.getName());
        if (functionLibInfo.getReturnDependencyInfo().isInputDep()) {
            m_inputSources.push_back(m_pdg.getFunctionNode(const_cast<llvm::Function*>(&F)));
        }
        // TODO: think if should add formal nodes or actual nodes?
        for (auto argDep : functionLibInfo.getArgumentDependencies()) {
            if (argDep.second.dependency == InputDepInfo::INPUT_DEP) {
                int argIdx = argDep.first;
                llvm::Argument* arg = functionLibInfo.getArgument(argIdx);
                assert(fPDG->hasFormalArgNode(arg));
                auto argNode = fPDG->getFormalArgNode(arg);
                m_inputSources.push_back(argNode);
            }
        }
        // TODO: process callback arguments?
    }
}

} // namespace input_dependency

