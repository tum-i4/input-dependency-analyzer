#pragma once

#include <fstream>

namespace llvm {
class Instruction;
}

namespace input_dependency {

class LoggingUtils
{
public:
    static void log_instruction_dbg_info(llvm::Instruction& instr, std::ofstream& log_stream);
};
}
