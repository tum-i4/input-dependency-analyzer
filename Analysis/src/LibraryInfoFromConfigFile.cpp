#include "input-dependency/Analysis/LibraryInfoFromConfigFile.h"
#include "input-dependency/Analysis/LibFunctionInfo.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <fstream>

namespace input_dependency {

void LibraryInfoFromConfigFile::setup()
{
    std::ifstream ifs (m_config_file, std::ifstream::in);
    if (!ifs.is_open()) {
        llvm::dbgs() << "Could not open file " << m_config_file << "\n";
        return;
    }
    json root;
    ifs >> root;
    llvm::dbgs() << "Parsing library functions config\n";
    const json functions = root["functions"];
    for (unsigned i = 0; i < functions.size(); ++i) {
        const json function_val = functions[i];
        add_library_function(function_val);
    }
}

void LibraryInfoFromConfigFile::add_library_function(const json& function_value)
{
    const std::string& f_name = function_value["name"];
    auto arg_deps_pos = function_value.find("deps");
    LibFunctionInfo libInfo(f_name);
    if (arg_deps_pos != function_value.end()) {
        json arg_deps = *arg_deps_pos;
        parse_dependencies(libInfo, arg_deps);
    }
    const auto& callbackIndices = get_callback_indices(function_value);
    if (!callbackIndices.empty()) {
        libInfo.setCallbackArgumentIndices(callbackIndices);
    }
    m_libFunctionInfoProcessor(std::move(libInfo));
}

void LibraryInfoFromConfigFile::parse_dependencies(LibFunctionInfo& libInfo, const json& arg_deps)
{
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    LibFunctionInfo::LibArgDepInfo returnDeps;
    for (unsigned i = 0; i < arg_deps.size(); ++i) {
        json arg_dep = arg_deps[i];
        for (auto it = arg_dep.begin(); it != arg_dep.end(); ++it) {
            const auto& entry_deps = get_entry_dependencies(it.value());
            LibFunctionInfo::LibArgDepInfo argDepInfo;
            if (entry_deps.dependency != DepInfo::UNKNOWN) {
                argDepInfo.dependency = entry_deps.dependency;
            } else {
                argDepInfo.argumentDependencies = std::move(entry_deps.argumentDependencies);
            }

            if (it.key() == "return") {
                returnDeps = argDepInfo;
            } else {
                try {
                    int arg_num = std::stoi(it.key());
                    argDeps.insert(std::make_pair(arg_num, argDepInfo));
                } catch (const std::exception& e) {
                    llvm::dbgs() << "Invalid entry " << it.key() << "\n";
                }
            }
        }
    }
    libInfo.setArgumentDeps(argDeps);
    libInfo.setReturnDeps(returnDeps);
}

LibFunctionInfo::LibArgDepInfo LibraryInfoFromConfigFile::get_entry_dependencies(const json& entry)
{
    DepInfo::Dependency dep = DepInfo::UNKNOWN;
    std::unordered_set<int> arg_deps;
    for (const auto& val : entry) {
        if (val.is_number()) {
            arg_deps.insert(val.get<int>());
        } else {
            const std::string& val_str = val;
            if (val_str == "dep") {
                dep = DepInfo::INPUT_DEP;
            } else if (val_str == "indep") {
                dep = DepInfo::INPUT_INDEP;
            }
            break;
        }
    }
    return LibFunctionInfo::LibArgDepInfo{dep, arg_deps};
}

LibFunctionInfo::ArgumentIndices LibraryInfoFromConfigFile::get_callback_indices(const json& entry)
{
    LibFunctionInfo::ArgumentIndices indices;
    auto callback_pos = entry.find("callback_arguments");
    if (callback_pos == entry.end()) {
        return indices;
    }
    auto callback_indices = *callback_pos;
    for (const auto& val : callback_indices) {
        if (!val.is_number()) {
            llvm::dbgs() << "Invalid value type for callback argument. Expects a number\n";
            continue;
        }
        indices.insert(val.get<int>());
    }
    return indices;
}

}

