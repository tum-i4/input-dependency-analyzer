#include "LLVMIntrinsicsInfo.h"
#include "LibFunctionInfo.h"

namespace input_dependency {

namespace intrinsics {

const std::string& memcpy = "memcpy";

}

void LLVMIntrinsicsInfo::setup()
{
    add_memcpy();
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

}
