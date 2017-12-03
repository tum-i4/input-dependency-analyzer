#pragma once

#include "LibraryInfoCollector.h"

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

private:
    void add_memcpy();
    void add_memset();
    //TODO: add more
};

} // namespace input_dependency
