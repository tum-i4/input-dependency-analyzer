#include "FunctionDOTGraphPrinter.h"

#include "FunctionAnaliser.h"

#include "Utils.h"

#include "llvm/IR/Function.h"
#include "llvm/Analysis/CFGPrinter.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Support/FileSystem.h"

#include "llvm/PassRegistry.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"


namespace llvm {

template <>
struct GraphTraits<input_dependency::FunctionAnaliser*> : public GraphTraits<BasicBlock*>
{
    static NodeType* getEntryNode(input_dependency::FunctionAnaliser* FA) {return &FA->getFunction()->getEntryBlock();}
    typedef Function::iterator nodes_iterator;
    static nodes_iterator nodes_begin(input_dependency::FunctionAnaliser* FA) {return FA->getFunction()->begin();}
    static nodes_iterator nodes_end(input_dependency::FunctionAnaliser* FA) {return FA->getFunction()->end();}
    static size_t size(input_dependency::FunctionAnaliser* FA) {return FA->getFunction()->size();}
};

template<>
struct GraphTraits<const input_dependency::FunctionAnaliser*> : public GraphTraits<const BasicBlock*>
{
    static NodeType* getEntryNode(const input_dependency::FunctionAnaliser* FA) {return &FA->getFunction()->getEntryBlock();}
    typedef Function::const_iterator nodes_iterator;
    static nodes_iterator nodes_begin(const input_dependency::FunctionAnaliser* FA) {return FA->getFunction()->begin();}
    static nodes_iterator nodes_end(const input_dependency::FunctionAnaliser* FA) {return FA->getFunction()->end();}
    static size_t size(const input_dependency::FunctionAnaliser* FA) {return FA->getFunction()->size();}
};

template<>
class DOTGraphTraits<const input_dependency::FunctionAnaliser*> : public DefaultDOTGraphTraits
{
public:
    DOTGraphTraits (bool isSimple=false)
        : DefaultDOTGraphTraits(isSimple)
    {}

    static std::string getGraphName(const input_dependency::FunctionAnaliser* F) {
        return "CFG for '" + F->getFunction()->getName().str() + "' function";
    }

    static std::string getSimpleNodeLabel(const BasicBlock *Node,
                                          const input_dependency::FunctionAnaliser* analiser)
    {
        if (!Node->getName().empty()) {
            return Node->getName().str();
        }

        std::string Str;
        raw_string_ostream OS(Str);

        Node->printAsOperand(OS, false);
        return OS.str();
    }

    static std::string getCompleteNodeLabel(const BasicBlock *Node,
                                            const input_dependency::FunctionAnaliser* analiser)
    {
        enum { MaxColumns = 80 };
        std::string Str;
        raw_string_ostream OS(Str);

        if (analiser->isInputDependentBlock(const_cast<llvm::BasicBlock*>(Node))) {
            OS << "*** " << Node->getName() << "\n";
        } else {
            OS << Node->getName() << "\n";
        }
        for (auto& instr : *Node) {
            if (analiser->isInputDependent(&instr)) {
                OS << "*** ";
            }
            OS << instr;
            OS << "\n";
        }
        std::string OutStr = OS.str();
        if (OutStr[0] == '\n') OutStr.erase(OutStr.begin());

        // Process string output to make it nicer...
        unsigned ColNum = 0;
        unsigned LastSpace = 0;
        for (unsigned i = 0; i != OutStr.length(); ++i) {
            if (OutStr[i] == '\n') {                            // Left justify
                OutStr[i] = '\\';
                OutStr.insert(OutStr.begin()+i+1, 'l');
                ColNum = 0;
                LastSpace = 0;
            } else if (OutStr[i] == ';') {                      // Delete comments!
                unsigned Idx = OutStr.find('\n', i+1);            // Find end of line
                OutStr.erase(OutStr.begin()+i, OutStr.begin()+Idx);
                --i;
            } else if (ColNum == MaxColumns) {                  // Wrap lines.
                // Wrap very long names even though we can't find a space.
                if (!LastSpace)
                    LastSpace = i;
                OutStr.insert(LastSpace, "\\l...");
                ColNum = i - LastSpace;
                LastSpace = 0;
                i += 3; // The loop will advance 'i' again.
            }
            else
                ++ColNum;
            if (OutStr[i] == ' ')
                LastSpace = i;
        }
        return OutStr;
    }

    static std::string getEdgeSourceLabel(const BasicBlock *Node,
                                          succ_const_iterator I) {
        // Label source of conditional branches with "T" or "F"
        if (const BranchInst *BI = dyn_cast<BranchInst>(Node->getTerminator()))
            if (BI->isConditional())
                return (I == succ_begin(Node)) ? "T" : "F";

        // Label source of switch edges with the associated value.
        if (const SwitchInst *SI = dyn_cast<SwitchInst>(Node->getTerminator())) {
            unsigned SuccNo = I.getSuccessorIndex();

            if (SuccNo == 0) return "def";

            std::string Str;
            raw_string_ostream OS(Str);
            SwitchInst::ConstCaseIt Case =
                SwitchInst::ConstCaseIt::fromSuccessorIndex(SI, SuccNo);
            OS << Case.getCaseValue()->getValue();
            return OS.str();
        }
        return "";
    }

    std::string getNodeLabel(const BasicBlock* Node,
                             const input_dependency::FunctionAnaliser* Graph)
    {
        if (isSimple()) {
            return getSimpleNodeLabel(Node, Graph);
        } else {
            return getCompleteNodeLabel(Node, Graph);
        }
    }
};

}

namespace input_dependency {

char FunctionDOTGraphPrinter::ID = 0;

bool FunctionDOTGraphPrinter::runOnFunction(llvm::Function &F)
{
    if (Utils::isLibraryFunction(&F, F.getParent())) {
        return false;
    }
    auto& Analysis = getAnalysis<InputDependencyAnalysis>();

    const auto& analysis_res = Analysis.getAnalysisInfo(&F);
    if (analysis_res == nullptr) {
        llvm::errs() << "Can't find analysis info for function\n";
        return false;
    }
    const FunctionAnaliser* Graph = analysis_res->toFunctionAnalysisResult();
    if (!Graph) {
        return false;
    }
    std::string Filename = "cfg." + F.getName().str() + ".dot";
    std::error_code EC;

    llvm::errs() << "Writing '" << Filename << "'...";

    llvm::raw_fd_ostream File(Filename, EC, llvm::sys::fs::F_Text);
    std::string GraphName = llvm::DOTGraphTraits<const FunctionAnaliser*>::getGraphName(Graph);
    std::string Title = GraphName + " for '" + F.getName().str() + "' function";

    if (!EC)
        llvm::WriteGraph(File, Graph, false, Title);
    else
        llvm::errs() << "  error opening file for writing!";
    llvm::errs() << "\n";

    return false;
}

void FunctionDOTGraphPrinter::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.setPreservesAll();
    AU.addRequired<InputDependencyAnalysis>();
}

static llvm::RegisterPass<FunctionDOTGraphPrinter> X("print-dot","Print dot with input dependency results");
} // namespace input_dependency

