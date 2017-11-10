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

private:
    bool goto_unsafe;
};

} // namespace input_dependency

