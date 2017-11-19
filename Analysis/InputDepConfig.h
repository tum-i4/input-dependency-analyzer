#pragma once

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

private:
    bool goto_unsafe;
    std::string lib_config_file;
};

} // namespace input_dependency

