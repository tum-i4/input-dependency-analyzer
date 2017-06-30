#include "LoggingUtils.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace input_dependency {

void LoggingUtils::log_instruction_dbg_info(llvm::Instruction& instr, std::ofstream& log_stream)
{
    const auto& debug_loc = instr.getDebugLoc();
    if (debug_loc.get() == nullptr) {
        llvm::dbgs() << "No debug info for instruction " << instr << "\n";
        return;
    }
    auto file = debug_loc.get()->getScope()->getFile();
    const std::string file_name = file->getFilename();
    log_stream << "file: " << file_name
               << " line: "
               << debug_loc.getLine()
               << " column: "
               << debug_loc.getCol() << "\n";

    //llvm::dbgs() << instr << "\n";
}


}


