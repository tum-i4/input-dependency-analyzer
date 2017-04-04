#include "LibFunctionInfo.h"
#include "LibraryInfoManager.h"

#include "llvm/IR/Function.h"

#include <cassert>

namespace input_dependency {

LibraryInfoManager& LibraryInfoManager::get()
{
    static LibraryInfoManager libraryInfo;
    return libraryInfo;
}

LibraryInfoManager::LibraryInfoManager()
{
    setup();
}

void LibraryInfoManager::setup()
{
    // TODO: create and store info for library functions
}

bool LibraryInfoManager::hasLibFunctionInfo(const std::string& funcName) const
{
    return m_libraryInfo.find(funcName) != m_libraryInfo.end();
}

const LibFunctionInfo& LibraryInfoManager::getLibFunctionInfo(const std::string& funcName) const
{
    auto pos = m_libraryInfo.find(funcName);
    assert(pos != m_libraryInfo.end());
    return pos->second;
}

void LibraryInfoManager::resolveLibFunctionInfo(llvm::Function* F)
{
    const auto& libF = getLibFunctionInfo(F->getName());
    if (libF.isResolved()) {
        return;
    }
    const_cast<LibFunctionInfo&>(libF).resolve(F);
}

} // namespace input_dependency

