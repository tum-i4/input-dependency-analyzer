#include "input-dependency/Analysis/InputDepInstructionsRecorder.h"
#include "input-dependency/Analysis/LoggingUtils.h"

#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <fstream>

namespace input_dependency {

void InputDepInstructionsRecorder::record(llvm::Instruction* I)
{
    if (m_record) {
        m_input_dep_instructions.insert(I);
    }
}

void InputDepInstructionsRecorder::record(llvm::BasicBlock* B)
{
    if (m_record) {
        for (auto& I : *B) {
            m_input_dep_instructions.insert(&I);
        }
    }
}


void InputDepInstructionsRecorder::dump_dbg_info() const
{
    std::ofstream dbg_infostrm;
    dbg_infostrm.open("recorded_inputdeps.dbg");
    LoggingUtils logger;
    for (auto& I : m_input_dep_instructions) {
        logger.log_instruction_dbg_info(*I, dbg_infostrm);
    }
    logger.log_not_logged_count(dbg_infostrm);
    dbg_infostrm.close();
}

}

