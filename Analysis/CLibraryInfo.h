#pragma once

#include <functional>

namespace input_dependency {

class LibFunctionInfo;

class CLibraryInfo
{
public:
    using LibraryInfoCallback = std::function<void (LibFunctionInfo&& functionInfo)>;

public:
    CLibraryInfo(const LibraryInfoCallback& callback);

    CLibraryInfo(const CLibraryInfo& ) = delete;
    CLibraryInfo(CLibraryInfo&& ) = delete;
    CLibraryInfo& operator =(const CLibraryInfo& ) = delete;
    CLibraryInfo& operator =(CLibraryInfo&& ) = delete;

public:
    void setup();

private:
    void add_printf();
    void add_remove();
    void add_rename();
    void add_fflush();
    void add_fopen();
    void add_fropen();
    void add_snprintf();
    void add_sprintf();
    void add_sscanf();
    void add_puts();
    void add_atof();
    void add_atoi();
    void add_atol();
    void add_atoll();
    void add_getenv();
    void add_system();
    void add_abs();
    void add_labs();

private:
    const LibraryInfoCallback& m_libFunctionInfoProcessor;
};

} // namespace input_dependency

