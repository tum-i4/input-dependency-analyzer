#pragma once

#include <unordered_set>

#include "llvm/IR/Function.h"

namespace input_dependency {

/**
 * Configurations for input dependency pass run
 */
class InputDepConfig
{
public:
    static InputDepConfig& get()
    {
        static InputDepConfig config;
        return config;
    }

public:
    InputDepConfig() = default;

    bool is_goto_unsafe() const
    {
        return goto_unsafe;
    }

    void set_goto_unsafe(bool g_unsafe)
    {
        goto_unsafe = g_unsafe;
    }

    void set_lib_config_file(const std::string& config_file)
    {
        lib_config_file = config_file;
    }

    bool has_config_file() const
    {
        return !lib_config_file.empty();
    }

    const std::string& get_config_file() const
    {
        return lib_config_file;
    }

    void set_use_cache(bool cache)
    {
        use_cache = cache;
    }

    bool is_use_cache() const
    {
        return use_cache;
    }

    void add_skip_input_dep_function(llvm::Function* F)
    {
        skip_input_dep_functions.insert(F);
    }

    bool is_skip_input_dep_function(llvm::Function* F)
    {
        return skip_input_dep_functions.find(F) != skip_input_dep_functions.end();
    }

private:
    bool goto_unsafe;
    bool cache_input_dep;
    std::string lib_config_file;
    bool use_cache;
    std::unordered_set<llvm::Function*> skip_input_dep_functions;
};

} // namespace input_dependency

