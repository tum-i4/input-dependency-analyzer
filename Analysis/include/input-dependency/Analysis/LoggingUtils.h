#pragma once

#include <fstream>

namespace llvm {
class Instruction;
}

namespace input_dependency {

class LoggingUtils
{
public:
    LoggingUtils()
        : not_logged(0)
    {
    }

public:
    void log_instruction_dbg_info(llvm::Instruction& instr, std::ofstream& log_stream);
    void log_not_logged_count(std::ofstream& log_stream);

private:
    long unsigned not_logged;
};
}
