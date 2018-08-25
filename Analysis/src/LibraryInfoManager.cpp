#include "input-dependency/Analysis/LibFunctionInfo.h"
#include "input-dependency/Analysis/LibraryInfoManager.h"
#include "input-dependency/Analysis/CLibraryInfo.h"
#include "input-dependency/Analysis/STLStringInfo.h"
#include "input-dependency/Analysis/LibraryInfoFromConfigFile.h"
#include "input-dependency/Analysis/LLVMIntrinsicsInfo.h"
#include "input-dependency/Analysis/InputDepConfig.h"

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
    const auto& libFunctionCollector =
                    [this] (LibFunctionInfo&& libFunctionInfo) {
                        this->addLibFunctionInfo(std::move(libFunctionInfo));
                    };
    // C library functions
    CLibraryInfo clibInfo(libFunctionCollector);
    clibInfo.setup();

    STLStringInfo stlStringInfo(libFunctionCollector);
    stlStringInfo.setup();

    LLVMIntrinsicsInfo llvmIntrinsicsInfo(libFunctionCollector);
    llvmIntrinsicsInfo.setup();

    if (InputDepConfig::get().has_config_file()) {
        LibraryInfoFromConfigFile configInfo(libFunctionCollector, InputDepConfig::get().get_config_file());
        configInfo.setup();
    }
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

void LibraryInfoManager::resolveLibFunctionInfo(llvm::Function* F, const std::string& demangledName)
{
    const auto& libF = getLibFunctionInfo(demangledName);
    if (libF.isResolved()) {
        return;
    }
    const_cast<LibFunctionInfo&>(libF).resolve(F);
}

void LibraryInfoManager::addLibFunctionInfo(const LibFunctionInfo& funcInfo)
{
    m_libraryInfo.emplace(funcInfo.getName(), funcInfo);
}

void LibraryInfoManager::addLibFunctionInfo(LibFunctionInfo&& funcInfo)
{
    m_libraryInfo.emplace(funcInfo.getName(), std::move(funcInfo));
}

} // namespace input_dependency

