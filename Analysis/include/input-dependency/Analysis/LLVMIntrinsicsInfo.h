#pragma once

#include "input-dependency/Analysis/LibraryInfoCollector.h"

#include <functional>

namespace input_dependency {

class LLVMIntrinsicsInfo : public LibraryInfoCollector
{
public:
    LLVMIntrinsicsInfo(const LibraryInfoCallback& callback)
        : LibraryInfoCollector(callback)
    {
    }

public:
    void setup() override;

public:
    static std::string get_intrinsic_name(const std::string& name);

private:
    void add_memcpy();
    void add_memset();
    void add_declare();
    //TODO: add more
};

} // namespace input_dependency
