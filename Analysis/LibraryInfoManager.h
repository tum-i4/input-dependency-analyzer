#pragma once

#include <unordered_map>

namespace llvm {

class Function;

}

namespace input_dependency {

class LibFunctionInfo;

class LibraryInfoManager
{
public:
    using LibFunctionInfoMap = std::unordered_map<std::string, LibFunctionInfo>;

public:
    static LibraryInfoManager& get();

private:
    LibraryInfoManager();

    LibraryInfoManager(const LibraryInfoManager& ) = delete;
    LibraryInfoManager(LibraryInfoManager&& ) = delete;
    LibraryInfoManager& operator =(const LibraryInfoManager& ) = delete;
    LibraryInfoManager& operator =(LibraryInfoManager&& ) = delete;

private:
    void setup();

public:
    bool hasLibFunctionInfo(const std::string& funcName) const;
    const LibFunctionInfo& getLibFunctionInfo(const std::string& funcName) const;

public:
    void resolveLibFunctionInfo(llvm::Function* F) const;

private:
    void addLibFunctionInfo(const LibFunctionInfo& funcInfo);
    void addLibFunctionInfo(LibFunctionInfo&& funcInfo);

    LibFunctionInfo& getLibFunctionInfo(const std::string& funcName);

private:
    LibFunctionInfoMap m_libraryInfo;
}; // class LibraryInfoManager

} // namespace input_dependency

