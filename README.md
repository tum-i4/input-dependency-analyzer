# Building the project
To build the project llvm3.9 or above should be installed.

       mkdir build
       cd build
       cmake $PATH_TO_SRC
       make

# Input Dependency Analysis pass

Input dependency analysis pass is a context sensitive, flow sensitive llvm analysis pass. It gets as an input llvm bitcode and collects information about input dependent and input independent instructions. It considers both data flow dependencies and control flow dependencies. An instruction is said to be input dependent by data flow, if any of its arguments is input dependent. An instruction is input dependent by control flow if it is in a branch, which condition is input dependent. Primary sources of inputs are arguments of main functions. All external functions which are considered as input sources, if not stated otherwise in the configuration files.

# Runing input dependency analysis

        opt -load $PATH_TO_LIB/libInputDependency.so bitcode.bc -input-dep -o out_bitcode.bc
       
# Using input dependency in your pass

To use Input dependency analysis information in your pass you need to register it as a required pass

        AU.addRequired<input_dependency::InputDependencyAnalysis>();

Then get it's information:
    
        const auto& input_dependency_info = getAnalysis<input_dependency::InputDependencyAnalysis>();

InputDependencyAnalysis provides interface to request information about instruction input dependency:

        bool isInputDependent(llvm::Instruction* instr) const;
        bool isInputIndependent(llvm::Instruction* instr) const;

# Debug passes
- Input dependent functions finding pass is an analysis pass that finds input dependent functions using input dependency pass results. A function is considered to be input dependent if any call site in its call chain is input dependent. This pass can be used in OH pass to skip input dependent functions as their calls are non deterministic. 
    
        opt -load $PATH_TO_LIB/libInputDependency.so bitcode.bc -function-call-info -o out.bc

- Dot printer pass dumps CFG of each function into a dot graph file, including input dependency information for each instruction.

    opt -load $PATH_TO_LIB/libInputDependency.so bitcode.bc -print-dot -o out.bc
A dot file can further be interpreted as image. Input dependent instructions will be marked with \*\*\* in image.

        dot -Tpng dotfilename -o imagename
    
- To get simple statistic information about input dependent instructions in a bitcode run:

        opt -load $PATH_TO_LIB/libInputDependency.so bitcode.bc -inputdep-statistics -o out.bc
 The output of the pass is number of input dependent and input independent instructions in each function, as well as the percentage of input dependent instructions. The pass also outputs this data for whole module.
 
 - Input dependency debug info pass outputs the source code line and column for each input dependent instruction. To have this pass run, the bitcode should have been compiled with -g flag.
 
        opt -load $PATH_TO_LIB/libInputDependency.so bitcode.bc -inputdep-dbginfo -o out.bc
    
- Library functions report pass reports all library functions in a module. A function is considered to be a library function if module does not contains the definition of that funciton. This pass can be usefull when adding library functions for a new project.

        opt -load $PATH_TO_LIB/libInputDependency.so bitcode.bc -lib-func-report -o out.bc

- Module size pass reports number of functions in a module. For each function it reports number of basic blocks, loops and instructions. This pass can be usefull for evaluation to find project size for different plots and tables. 

        opt -load $PATH_TO_LIB/libInputDependency.so bitcode.bc -mod-size -o out.bc
    
 
# Transformation passes

- Function clonning pass is a transformation pass that clones functions for different set of input dependent arguments. For example given following functions and calls:

        void f(int a, int b)
        {
        }
        
        int x = input;
        f(3, x);
        f(2, 3);
        
Input dependency pass will consider the second argument of function f to be input dependent as there is a call site where it is input dependent. Function clonning pass will create two clones of f - f1 and f2, where f1 will be called with the first set of arguments, and f2 will be called with the second set of arguments. The pass will also change call sites to call corresponding clones.

        void f1(int a, int b)
        {
        }
        void f2(int a, int b)
        {
        }

        int x = input;
        f1(3, x);
        f2(2, 3);
    
To run the pass

        opt -load $PATH_TO_LIB/libInputDependency.so -load $PATH_TO_LIB/libTransforms.so bitcode.bc -clone-functions -o out.bc
        
- Function extraction pass is a transformation pass which extracts input dependent portions of a function to a separate function and adds calls to the extracted functions.

        void f ()
        {
            int x = 0;          (1)
            int y = input;      (2)
            int sum = x + y;    (3)
        }

Lines (2) and (3) are input dependent. The result of running this pass will be

        void f_extracted(int* a, int* b, int* c)
        {
            *b = input;
            *c = *b + *a;
        }
        
        void f()
        {
            int x = 0;
            int y;
            int sum;
            f_extracted(&x, &y, &sum);
        }
        
To run the pass

        opt -load $PATH_TO_LIB/libInputDependency.so -load $PATH_TO_LIB/libTransforms.so bitcode.bc -extract-functions -o out.bc
