#include "input-dependency/Analysis/LLVMIntrinsicsInfo.h"
#include "input-dependency/Analysis/LibFunctionInfo.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace input_dependency {

namespace intrinsics {

const std::string& memcpy = "memcpy";
const std::string& memset = "memset";
const std::string& declare = "declare";
}

std::string LLVMIntrinsicsInfo::get_intrinsic_name(const std::string& name)
{
    // As there are just a few intrinsics we can live with this implementation
    if (name.find(intrinsics::memcpy) != std::string::npos) {
        return intrinsics::memcpy;
    } else if (name.find(intrinsics::memset) != std::string::npos) {
        return intrinsics::memset;
    } else if (name.find(intrinsics::declare) != std::string::npos) {
        return intrinsics::declare;
    } else {
        llvm::dbgs() << "Non registered intrinsic " << name << "\n";
    }
    return "";
}

void LLVMIntrinsicsInfo::setup()
{
    add_memcpy();
    add_memset();
    add_declare();
}

void LLVMIntrinsicsInfo::add_memcpy()
{
    // @llvm.memcpy.p0i8.p0i8.i32(i8* <dest>, i8* <src>,
    //                                    i32 <len>, i32 <align>, i1 <isvolatile>)
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {1, 2}, argDeps);
    LibFunctionInfo memcpy(intrinsics::memcpy,
                           argDeps,
                           LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_INDEP});
    m_libFunctionInfoProcessor(std::move(memcpy));
}

void LLVMIntrinsicsInfo::add_memset()
{
    // @llvm.memset.p0i8.i32(i8* <dest>, i8 <val>,
    //                       i32 <len>, i32 <align>, i1 <isvolatile>)
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {1, 2}, argDeps);
    LibFunctionInfo memset(intrinsics::memset,
                           argDeps,
                           LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_INDEP});
    m_libFunctionInfoProcessor(std::move(memset));
}

void LLVMIntrinsicsInfo::add_declare()
{
    // void @llvm.dbg.declare(metadata, metadata, metadata)
    LibFunctionInfo declare(intrinsics::declare,
                            LibFunctionInfo::LibArgumentDependenciesMap(),
                            LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_INDEP});
    m_libFunctionInfoProcessor(std::move(declare));
}

}
