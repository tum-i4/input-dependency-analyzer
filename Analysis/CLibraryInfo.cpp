#include "CLibraryInfo.h"

namespace input_dependency {

namespace C_library {

const std::string& printf = "printf"; 
const std::string& remove = "remove"; 
const std::string& rename = "rename"; 
const std::string& fflush = "fflush"; 
const std::string& fopen = "fopen"; 
const std::string& freopen = "freopen"; 
const std::string& fwrite = "fwrite";
const std::string& fputc = "fputc";
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
const std::string& strlen = "strlen";

const std::string& malloc = "malloc";
const std::string& calloc = "calloc";
const std::string& memcpy = "memcpy";
const std::string& new_operator = "operator new(unsigned long)";

const std::string& free = "free";
const std::string& realloc = "realloc";
const std::string& fprintf = "fprintf";
const std::string& qsort = "qsort";
const std::string& log = "log";
const std::string& strcmp = "strcmp";
const std::string& strcpy = "strcpy";
const std::string& strcat = "strcat";
const std::string& fseek = "fseek";
const std::string& ftell = "ftell";
const std::string& rewind = "rewind";
const std::string& fread = "fread";
const std::string& fclose = "fclose";
} // namespace C_library

void CLibraryInfo::setup()
{
    add_printf();
    add_remove();
    add_rename();
    add_fflush();
    add_fopen();
    add_fropen();
    add_fwrite();
    add_fputc();
    add_snprintf();
    //add_sprintf();
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
    add_strlen();
    add_malloc();
    add_calloc();
    add_memcpy();

    add_new_operator();

    add_free();
    add_realloc();
    add_fprintf();
    add_qsort();
    add_log();
    add_strcmp();
    add_strcpy();
    add_strcat();
    add_fseek();
    add_ftell();
    add_rewind();
    add_fread();
    add_fclose();
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

void CLibraryInfo::add_fwrite()
{
    // size_t fwrite ( const void * ptr, size_t size, size_t count, FILE * stream );
    LibFunctionInfo fwriteInfo(C_library::fwrite,
                               LibFunctionInfo::LibArgumentDependenciesMap(),
                               LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_ARGDEP, {1, 2}});
    m_libFunctionInfoProcessor(std::move(fwriteInfo));
}

void CLibraryInfo::add_fputc()
{
    LibFunctionInfo fputc(C_library::fputc,
                          LibFunctionInfo::LibArgumentDependenciesMap(),
                          LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_DEP});
    m_libFunctionInfoProcessor(std::move(fputc));
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
    //addArgWithDeps(2, {0, 1}, argDeps);
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

void CLibraryInfo::add_strlen()
{
    LibFunctionInfo strlenInfo(C_library::strlen,
                               LibFunctionInfo::LibArgumentDependenciesMap(),
                               LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_ARGDEP, {0}});
    m_libFunctionInfoProcessor(std::move(strlenInfo));
}

void CLibraryInfo::add_malloc()
{
    LibFunctionInfo mallocInfo(C_library::malloc,
                               LibFunctionInfo::LibArgumentDependenciesMap(),
                               LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_INDEP});
    m_libFunctionInfoProcessor(std::move(mallocInfo));
}

void CLibraryInfo::add_calloc()
{
    // void* calloc (size_t num, size_t size);
    // The return value is non-deterministic. In case of failure returns null.
    // However of nullptr was returned it shouldn't be used anyway.
    LibFunctionInfo callocInfo(C_library::calloc,
                               LibFunctionInfo::LibArgumentDependenciesMap(),
                               LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_INDEP});
    m_libFunctionInfoProcessor(std::move(callocInfo));
}

void CLibraryInfo::add_memcpy()
{
    // void * memcpy ( void * destination, const void * source, size_t num );
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {1, 2}, argDeps);
    LibFunctionInfo memcpy(C_library::memcpy,
                           argDeps,
                           LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_INDEP});
    m_libFunctionInfoProcessor(std::move(memcpy));
}

void CLibraryInfo::add_new_operator()
{
    LibFunctionInfo newopInfo(C_library::new_operator,
                              LibFunctionInfo::LibArgumentDependenciesMap(),
                              LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_ARGDEP, {0}});
    m_libFunctionInfoProcessor(std::move(newopInfo));
}

void CLibraryInfo::add_free()
{
    LibFunctionInfo freeInfo(C_library::free,
                             LibFunctionInfo::LibArgumentDependenciesMap(),
                             LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_INDEP});
    m_libFunctionInfoProcessor(std::move(freeInfo));
}

void CLibraryInfo::add_realloc()
{
    // content of ptr is not changed
    // void* realloc (void* ptr, size_t size);
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {0, 1}, argDeps);
    LibFunctionInfo reallocInfo(C_library::realloc,
                                std::move(argDeps),
                                LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_ARGDEP, {0, 1}});
    m_libFunctionInfoProcessor(std::move(reallocInfo));
}

void CLibraryInfo::add_fprintf()
{
    // return false is non-deterministic depending on success or failure
    // int fprintf ( FILE * stream, const char * format, ... );
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {0, 1}, argDeps);
    //argDeps.emplace(2, LibArgDepInfo{input_dependency::DepInfo::INPUT_INDEP,});
    LibFunctionInfo fprintfInfo(C_library::fprintf,
                                std::move(argDeps),
                                LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_DEP});
    m_libFunctionInfoProcessor(std::move(fprintfInfo));
}

void CLibraryInfo::add_qsort()
{
    // does the first argument depend on compar???
    // void qsort (void* base, size_t num, size_t size, int (*compar)(const void*,const void*));
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {0, 1}, argDeps);
    LibFunctionInfo qsortInfo(C_library::qsort,
                              std::move(argDeps),
                              LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_INDEP});
    m_libFunctionInfoProcessor(std::move(qsortInfo));
}

void CLibraryInfo::add_log()
{
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    LibFunctionInfo logInfo(C_library::log,
                            LibFunctionInfo::LibArgumentDependenciesMap(),
                            LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_ARGDEP, {0}});
    m_libFunctionInfoProcessor(std::move(logInfo));
}

void CLibraryInfo::add_strcmp()
{
    //  int strcmp ( const char * str1, const char * str2 );
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    LibFunctionInfo strcmpInfo(C_library::strcmp,
                               LibFunctionInfo::LibArgumentDependenciesMap(),
                               LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_ARGDEP, {0, 1}});
    m_libFunctionInfoProcessor(std::move(strcmpInfo));
}

void CLibraryInfo::add_strcpy()
{
    // char * strcpy ( char * destination, const char * source );
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {1}, argDeps);
    LibFunctionInfo strcpyInfo(C_library::strcpy,
                               std::move(argDeps),
                               LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_ARGDEP, {1}});
    m_libFunctionInfoProcessor(std::move(strcpyInfo));
}

void CLibraryInfo::add_strcat()
{
    //char * strcat ( char * destination, const char * source );
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {1}, argDeps);
    LibFunctionInfo strcatInfo(C_library::strcat,
                               std::move(argDeps),
                               LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_ARGDEP, {1}});
    m_libFunctionInfoProcessor(std::move(strcatInfo));
}

void CLibraryInfo::add_fseek()
{
    // int fseek ( FILE * stream, long int offset, int origin );
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    // FILE* is not literally becomming input dependent.
    // However following functions, e.g. reads may be non deterministic, thus mark FILE input dependent.
    addArgWithDeps(0, {0, 1, 2}, argDeps);
    LibFunctionInfo fseekInfo(C_library::fseek,
                              std::move(argDeps),
                              LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_DEP});
    m_libFunctionInfoProcessor(std::move(fseekInfo));
}

void CLibraryInfo::add_ftell()
{
    // return value does not merely depend on FILE*. on failure it will return -1. thus marking it input dependent as is non-deterministic
    // long int ftell ( FILE * stream );
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {0}, argDeps);
    LibFunctionInfo ftellInfo(C_library::ftell,
                              std::move(argDeps),
                              LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_DEP});
    m_libFunctionInfoProcessor(std::move(ftellInfo));
}

void CLibraryInfo::add_rewind()
{
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {0}, argDeps);
    LibFunctionInfo rewindInfo(C_library::rewind,
                               std::move(argDeps),
                               LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_INDEP});
    m_libFunctionInfoProcessor(std::move(rewindInfo));
}

void CLibraryInfo::add_fread()
{
    // size_t fread ( void * ptr, size_t size, size_t count, FILE * stream );
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {1, 2, 3}, argDeps);
    // FILE position is advanced by number of bytes read, which in success is size * count
    addArgWithDeps(3, {2, 3}, argDeps);
    LibFunctionInfo freadInfo(C_library::fread,
                              std::move(argDeps),
                              LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_DEP});
    m_libFunctionInfoProcessor(std::move(freadInfo));
}

void CLibraryInfo::add_fclose()
{
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    // even if fails, FILE is cleared
    argDeps.insert(std::make_pair(0, LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_INDEP}));
    LibFunctionInfo fcloseInfo(C_library::fclose,
                               std::move(argDeps),
                               LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_DEP});
    m_libFunctionInfoProcessor(std::move(fcloseInfo));
}

} // namespace input_dependency

