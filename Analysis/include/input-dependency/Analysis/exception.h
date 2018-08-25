#pragma once

#include <exception>

namespace input_dependency {

class IrregularCFGException : public std::exception
{
public:
    explicit IrregularCFGException(const std::string& msg)
        : m_msg(msg)
    {
    }

    ~IrregularCFGException()
    {
    }

    const char* what() const throw() override
    {
        const std::string full_msg = msg_prefix + m_msg;
        return full_msg.c_str();
    }

private:
    static const std::string msg_prefix;

private:
    const std::string& m_msg;
};

const std::string IrregularCFGException::msg_prefix = "Irregular CFG.";
}

