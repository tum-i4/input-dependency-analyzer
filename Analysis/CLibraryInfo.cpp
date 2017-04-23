#include "CLibraryInfo.h"
#include "LibFunctionInfo.h"

namespace {

namespace C_library {

const std::string& printf = "printf"; 
const std::string& remove = "remove"; 
const std::string& rename = "rename"; 
const std::string& fflush = "fflush"; 
const std::string& fopen = "fopen"; 
const std::string& freopen = "freopen"; 
const std::string& snprintf = "snprintf"; 
const std::string& sprintf = "sprintf"; 
const std::string& sscanf = "sscanf"; 
const std::string& puts = "puts"; 
const std::string& atof = "atof";
const std::string& atoi = "atoi";
const std::string& atol = "atol";
const std::string& atoll = "atoll";
const std::string& getenv = "getenv";
const std::string& system = "system";
const std::string& abs = "abs";
const std::string& labs = "labs";

const std::string& new_operator = "operator new(unsigned long)";
} // namespace C_library

}

namespace input_dependency {

void CLibraryInfo::setup()
{
    add_printf();
    add_remove();
    add_rename();
    add_fflush();
    add_fopen();
    add_fropen();
    add_snprintf();
    add_sprintf();
    add_sscanf();
    add_puts();
    //<cstdlib>
    add_atof();
    add_atoi();
    add_atol();
    add_atoll(); //c++11
    add_getenv();
    add_system();
    add_abs();
    add_labs();

    add_new_operator();
}

void CLibraryInfo::add_printf()
{
    // int printf ( const char * format, ... );
    // printf does not change any of its arguments
    // return value of printf function is not deterministic
    LibFunctionInfo printfInfo(C_library::printf,
                               LibFunctionInfo::LibArgumentDependenciesMap(),
                               LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_DEP});
    m_libFunctionInfoProcessor(std::move(printfInfo));
}

void CLibraryInfo::add_remove()
{
    // int remove ( const char * filename );
    LibFunctionInfo removeInfo(C_library::remove,
                               LibFunctionInfo::LibArgumentDependenciesMap(),
                               LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_DEP});
    m_libFunctionInfoProcessor(std::move(removeInfo));
}

void CLibraryInfo::add_rename()
{
    LibFunctionInfo renameInfo(C_library::rename,
                               LibFunctionInfo::LibArgumentDependenciesMap(),
                               LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_DEP});
    m_libFunctionInfoProcessor(std::move(renameInfo));
}

void CLibraryInfo::add_fflush()
{
    // int fflush ( FILE * stream );
    LibFunctionInfo fflushInfo(C_library::fflush,
                               LibFunctionInfo::LibArgumentDependenciesMap(),
                               LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_DEP});
    m_libFunctionInfoProcessor(std::move(fflushInfo));
}

void CLibraryInfo::add_fopen()
{
    // FILE * fopen ( const char * filename, const char * mode );
    LibFunctionInfo fopenInfo(C_library::fopen,
                              LibFunctionInfo::LibArgumentDependenciesMap(),
                              LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_DEP});
    m_libFunctionInfoProcessor(std::move(fopenInfo));
}

void CLibraryInfo::add_fropen()
{
    // FILE * freopen ( const char * filename, const char * mode, FILE * stream );
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(2, {0, 1}, argDeps);
    LibFunctionInfo freopenInfo(C_library::fopen,
                                std::move(argDeps),
                                LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_DEP});
    m_libFunctionInfoProcessor(std::move(freopenInfo));
}

void CLibraryInfo::add_snprintf()
{
    // int snprintf ( char * s, size_t n, const char * format, ... );
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {1, 2}, argDeps);
    LibFunctionInfo snprintfInfo(C_library::snprintf,
                                 std::move(argDeps),
                                 LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_DEP});
    m_libFunctionInfoProcessor(std::move(snprintfInfo));
}

void CLibraryInfo::add_sprintf()
{
    // int sprintf ( char * str, const char * format, ... );
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {1}, argDeps);
    LibFunctionInfo sprintfInfo(C_library::sprintf,
                                std::move(argDeps),
                                LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_DEP});
    m_libFunctionInfoProcessor(std::move(sprintfInfo));
}

void CLibraryInfo::add_sscanf()
{
    // int sscanf ( const char * s, const char * format, ...);
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(2, {0, 1}, argDeps);
    LibFunctionInfo sscanfInfo(C_library::sscanf,
                               std::move(argDeps),
                               LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_DEP});
    m_libFunctionInfoProcessor(std::move(sscanfInfo));
}

void CLibraryInfo::add_puts()
{
    // int puts ( const char * str );
    LibFunctionInfo putsInfo(C_library::puts,
                             LibFunctionInfo::LibArgumentDependenciesMap(),
                             LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_DEP});
    m_libFunctionInfoProcessor(std::move(putsInfo));
}

void CLibraryInfo::add_atof()
{
    LibFunctionInfo atofInfo(C_library::atof,
                             LibFunctionInfo::LibArgumentDependenciesMap(),
                             LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_ARGDEP, {0}});
    m_libFunctionInfoProcessor(std::move(atofInfo));
}

void CLibraryInfo::add_atoi()
{
    LibFunctionInfo atoiInfo(C_library::atoi,
                             LibFunctionInfo::LibArgumentDependenciesMap(),
                             LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_ARGDEP, {0}});
    m_libFunctionInfoProcessor(std::move(atoiInfo));
}

void CLibraryInfo::add_atol()
{
    LibFunctionInfo atolInfo(C_library::atol,
                             LibFunctionInfo::LibArgumentDependenciesMap(),
                             LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_ARGDEP, {0}});
    m_libFunctionInfoProcessor(std::move(atolInfo));
}

void CLibraryInfo::add_atoll()
{
    LibFunctionInfo atollInfo(C_library::atoll,
                              LibFunctionInfo::LibArgumentDependenciesMap(),
                              LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_ARGDEP, {0}});
    m_libFunctionInfoProcessor(std::move(atollInfo));
}

void CLibraryInfo::add_getenv()
{
    LibFunctionInfo getenvInfo(C_library::getenv,
                              LibFunctionInfo::LibArgumentDependenciesMap(),
                              LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_DEP});
    m_libFunctionInfoProcessor(std::move(getenvInfo));
}

void CLibraryInfo::add_system()
{
    LibFunctionInfo systemInfo(C_library::system,
                               LibFunctionInfo::LibArgumentDependenciesMap(),
                               LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_DEP});
    m_libFunctionInfoProcessor(std::move(systemInfo));
}

void CLibraryInfo::add_abs()
{
    LibFunctionInfo absInfo(C_library::abs,
                            LibFunctionInfo::LibArgumentDependenciesMap(),
                            LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_ARGDEP, {0}});
    m_libFunctionInfoProcessor(std::move(absInfo));
}

void CLibraryInfo::add_labs()
{
    LibFunctionInfo labsInfo(C_library::labs,
                             LibFunctionInfo::LibArgumentDependenciesMap(),
                             LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_ARGDEP, {0}});
    m_libFunctionInfoProcessor(std::move(labsInfo));
}

void CLibraryInfo::add_new_operator()
{
    LibFunctionInfo newopInfo(C_library::new_operator,
                              LibFunctionInfo::LibArgumentDependenciesMap(),
                              LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_ARGDEP, {0}});
    m_libFunctionInfoProcessor(std::move(newopInfo));
}

} // namespace input_dependency

