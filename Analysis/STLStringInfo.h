#pragma once

#include "LibraryInfoCollector.h"

#include <functional>

namespace input_dependency {

class STLStringInfo : public LibraryInfoCollector
{
public:
    STLStringInfo(const LibraryInfoCallback& callback)
        : LibraryInfoCollector(callback)
    {
    }

public:
    void setup() override;

private:
    void add_copy_constructor();
    void add_substring_constructor();
    void add_destructor();
    void add_assignment_operator();
    void add_assign_char_array();
    void add_assign_char();
    void add_size();
    void add_length();
    void add_max_size();
    void add_capacity();
    void add_empty();
    void add_resize();
    void add_reserve();

    void add_throw_length_error();

    void add_char_traits_length();
    void add_char_traits_copy();
    void add_char_traits_char_assign();
    void add_char_traits_array_assign();
};

} // namespace input_dependency

