#pragma once

#include "LibraryInfoCollector.h"

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

    void add_malloc();
    void add_calloc();

    // not sure if this should be here
    void add_new_operator();
};

} // namespace input_dependency

