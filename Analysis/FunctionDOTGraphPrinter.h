#include "InputDependencyAnalysis.h"

namespace llvm {
class Function;
}

namespace input_dependency {

class FunctionDOTGraphPrinter : public llvm::FunctionPass
{
public:
    static char ID;
    FunctionDOTGraphPrinter()
        : llvm::FunctionPass(ID)
    {
    }

    bool runOnFunction(llvm::Function &F) override;
    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;

};

} // unnamed namespace

