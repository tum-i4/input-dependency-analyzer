#pragma once

#include <string>

/// Declarations for constant strings used in the project
namespace input_dependency {

/// names for metadata
class metadata_strings {
public:
    const static std::string cached_input_dep;
    const static std::string input_dep_function;
    const static std::string input_indep_function;
    const static std::string input_dep_block;
    const static std::string input_indep_block;
    const static std::string input_dep_instr;
    const static std::string input_indep_instr;
    const static std::string control_dep_instr;
    const static std::string data_dep_instr;
    const static std::string data_indep_instr;
    const static std::string global_dep_instr;
    const static std::string argument_dep_instr;
    const static std::string unknown;
    const static std::string unreachable;
    const static std::string extracted;
};

} // namespace input_dependency

