#pragma once

#include <unordered_set>

namespace llvm {
class Instruction;
class BasicBlock;
}

namespace input_dependency {

class InputDepInstructionsRecorder
{
public:
    static InputDepInstructionsRecorder& get()
    {
        static InputDepInstructionsRecorder recorder;
        return recorder;
    }

private:
    InputDepInstructionsRecorder() = default;

public:
    void set_record()
    {
        m_record = true;
    }

    void reset_record()
    {
        m_record = false;
    }

    void reset()
    {
        reset_record();
        m_input_dep_instructions.clear();
    }
    
    void record(llvm::Instruction* I);
    void record(llvm::BasicBlock* B);

    void dump_dbg_info() const;

private:
    std::unordered_set<llvm::Instruction*> m_input_dep_instructions;
    bool m_record;
};

}

