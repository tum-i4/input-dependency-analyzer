#pragma once

#include "input-dependency/Analysis/LibraryInfoCollector.h"

#include <functional>

namespace input_dependency {

class CLibraryInfo : public LibraryInfoCollector
{
public:
    CLibraryInfo(const LibraryInfoCallback& callback)
        : LibraryInfoCollector(callback)
    {
    }

public:
    void setup() override;

private:
    void add_printf();
    void add_remove();
    void add_rename();
    void add_fflush();
    void add_fopen();
    void add_fropen();
    void add_fwrite();
    void add_fputc();
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
    void add_strlen();
    void add_setlocale();

    void add_malloc();
    void add_calloc();
    void add_memcpy();

    // not sure if this should be here
    void add_new_operator();

    void add_free();
    void add_realloc();
    void add_fprintf();
    void add_qsort();
    void add_log();
    void add_strcmp();
    void add_strcpy();
    void add_strcat();
    void add_fseek();
    void add_ftell();
    void add_rewind();
    void add_fread();
    void add_fclose();
};

} // namespace input_dependency

