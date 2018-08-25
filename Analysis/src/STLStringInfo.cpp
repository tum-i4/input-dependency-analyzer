#include "input-dependency/Analysis/STLStringInfo.h"
#include "input-dependency/Analysis/LibFunctionInfo.h"

namespace input_dependency {

namespace stl_basic_string {

// Demangled names does not contain the first argument, which is this.
// We'll consider that, when adding information
const std::string copy_constructor = "std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::basic_string(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&)";

const std::string substring_constructor = "std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::basic_string(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&, unsigned long, unsigned long, std::__1::allocator<char> const&)";

const std::string destructor = "std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::~basic_string()";

const std::string assignment_operator = "std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::operator=(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&)";

const std::string assign_char_array = "std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::assign(char const*)";

const std::string assign_char = "std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::operator=(char)";

// most probably these functions are going to be inlined
const std::string size = "std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::size() const";
const std::string length = "std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::length() const";
const std::string max_size = "std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::max_size() const";
const std::string capacity = "std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::capacity() const";
const std::string empty = "std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::empty() const";


const std::string resize = "std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::resize(unsigned long, char)";
const std::string reserve = "std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::reserve(unsigned long)";

const std::string throw_length_error = "std::__1::__basic_string_common<true>::__throw_length_error() const";
}

namespace stl_char_traits {

// These are static functions, thus there is no "this" argument
const std::string length = "std::__1::char_traits<char>::length(char const*)";
const std::string copy = "std::__1::char_traits<char>::copy(char*, char const*, unsigned long)";
const std::string assign_char = "std::__1::char_traits<char>::assign(char&, char const&)";
const std::string assign_array = "std::__1::char_traits<char>::assign(char*, unsigned long, char)";

}

void STLStringInfo::setup()
{
    add_copy_constructor();
    add_substring_constructor();
    add_destructor();
    add_assignment_operator();
    add_assign_char_array();
    add_assign_char();

    add_size();
    add_length();
    add_max_size();
    add_capacity();
    add_empty();
    add_resize();
    add_reserve();

    add_throw_length_error();

    add_char_traits_length();
    add_char_traits_copy();
    add_char_traits_char_assign();
    add_char_traits_array_assign();
}

void STLStringInfo::add_copy_constructor()
{
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {1}, argDeps);
    LibFunctionInfo info(stl_basic_string::copy_constructor,
                         argDeps,
                         LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_INDEP});
    m_libFunctionInfoProcessor(std::move(info));
}

void STLStringInfo::add_substring_constructor()
{
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {1, 2, 3}, argDeps);
    LibFunctionInfo info(stl_basic_string::substring_constructor,
                         argDeps,
                         LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_INDEP});
    m_libFunctionInfoProcessor(std::move(info));
}

void STLStringInfo::add_destructor()
{
    LibFunctionInfo info(stl_basic_string::destructor,
                         LibFunctionInfo::LibArgumentDependenciesMap(),
                         LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_INDEP});
    m_libFunctionInfoProcessor(std::move(info));
}

void STLStringInfo::add_assignment_operator()
{
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {1}, argDeps);
    LibFunctionInfo info(stl_basic_string::assignment_operator,
                         argDeps,
                         LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_INDEP});
    m_libFunctionInfoProcessor(std::move(info));
}

void STLStringInfo::add_assign_char_array()
{
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {1}, argDeps);
    LibFunctionInfo info(stl_basic_string::assign_char_array,
                         argDeps,
                         LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_INDEP});
    m_libFunctionInfoProcessor(std::move(info));
}

void STLStringInfo::add_assign_char()
{
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {1}, argDeps);
    LibFunctionInfo info(stl_basic_string::assign_char,
                         argDeps,
                         LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_INDEP});
    m_libFunctionInfoProcessor(std::move(info));
}

void STLStringInfo::add_size()
{
    LibFunctionInfo info(stl_basic_string::size,
                         LibFunctionInfo::LibArgumentDependenciesMap(),
                         LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_ARGDEP, {0}});
    m_libFunctionInfoProcessor(std::move(info));
}

void STLStringInfo::add_length()
{
    LibFunctionInfo info(stl_basic_string::length,
                         LibFunctionInfo::LibArgumentDependenciesMap(),
                         LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_ARGDEP, {0}});
    m_libFunctionInfoProcessor(std::move(info));
}

void STLStringInfo::add_max_size()
{
    LibFunctionInfo info(stl_basic_string::max_size,
                         LibFunctionInfo::LibArgumentDependenciesMap(),
                         LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_ARGDEP, {0}});
    m_libFunctionInfoProcessor(std::move(info));
}

void STLStringInfo::add_capacity()
{
    LibFunctionInfo info(stl_basic_string::capacity,
                         LibFunctionInfo::LibArgumentDependenciesMap(),
                         LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_ARGDEP, {0}});
    m_libFunctionInfoProcessor(std::move(info));
}

void STLStringInfo::add_empty()
{
    LibFunctionInfo info(stl_basic_string::empty,
                         LibFunctionInfo::LibArgumentDependenciesMap(),
                         LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_ARGDEP, {0}});
    m_libFunctionInfoProcessor(std::move(info));
}

void STLStringInfo::add_resize()
{
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {1, 2}, argDeps);
    LibFunctionInfo info(stl_basic_string::resize,
                         argDeps,
                         LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_INDEP});
    m_libFunctionInfoProcessor(std::move(info));
}

void STLStringInfo::add_reserve()
{
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {1}, argDeps);
    LibFunctionInfo info(stl_basic_string::reserve,
                         argDeps,
                         LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_INDEP});
    m_libFunctionInfoProcessor(std::move(info));
}

void STLStringInfo::add_throw_length_error()
{
    LibFunctionInfo info(stl_basic_string::throw_length_error,
                         LibFunctionInfo::LibArgumentDependenciesMap(),
                         LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_INDEP});
    m_libFunctionInfoProcessor(std::move(info));
}

void STLStringInfo::add_char_traits_length()
{
    // static size_t length (const char_type* s);
    LibFunctionInfo info(stl_char_traits::length,
                         LibFunctionInfo::LibArgumentDependenciesMap(),
                         LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_ARGDEP, {0}});
    m_libFunctionInfoProcessor(std::move(info));
}

void STLStringInfo::add_char_traits_copy()
{
    // static char_type* copy (char_type* dest, const char_type* src, size_t n);
    // note here return value depends on dest, which is the same as depending on src and n
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {1, 2}, argDeps);
    LibFunctionInfo info(stl_char_traits::copy,
                         argDeps,
                         LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_ARGDEP, {1, 2}});
    m_libFunctionInfoProcessor(std::move(info));
}

void STLStringInfo::add_char_traits_char_assign()
{
    // static void assign (char_type& r, const char_type& c);
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {1}, argDeps);
    LibFunctionInfo info(stl_char_traits::assign_char,
                         argDeps,
                         LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_INDEP});
    m_libFunctionInfoProcessor(std::move(info));
}

void STLStringInfo::add_char_traits_array_assign()
{
    // static char_type assign (char_type* p, site_t n, char_type c);
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    addArgWithDeps(0, {1, 2}, argDeps);
    LibFunctionInfo info(stl_char_traits::assign_array,
                         argDeps,
                         LibFunctionInfo::LibArgDepInfo{DepInfo::INPUT_INDEP});
    m_libFunctionInfoProcessor(std::move(info));
}

}

