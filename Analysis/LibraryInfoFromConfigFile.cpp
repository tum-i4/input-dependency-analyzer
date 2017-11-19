#include "LibraryInfoFromConfigFile.h"
#include "LibFunctionInfo.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "json/reader.h"
#include "json/value.h"

#include <fstream>

namespace input_dependency {

void LibraryInfoFromConfigFile::setup()
{
    Json::Value root;
    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    JSONCPP_STRING errs;

    std::ifstream ifs (m_config_file, std::ifstream::in);
    bool ok = parseFromStream(builder, ifs, &root, &errs);
    if (!ok) {
        llvm::dbgs() << "Failed to parse library function configuration file " << m_config_file << "\n";
        return;
    }

    const Json::Value functions = root["functions"];
    for (unsigned i = 0; i < functions.size(); ++i) {
        const Json::Value function_val = functions[i];
        add_library_function(function_val);
    }
}

void LibraryInfoFromConfigFile::add_library_function(const Json::Value& function_value)
{
    const std::string& f_name = function_value["name"].asString();
    Json::Value arg_deps = function_value["deps"];
    LibFunctionInfo::LibArgumentDependenciesMap argDeps;
    LibFunctionInfo::LibArgDepInfo returnDeps;
    for (unsigned i = 0; i < arg_deps.size(); ++i) {
        Json::Value arg_dep = arg_deps[i];
        for (const auto& key : arg_dep.getMemberNames()) {
            const auto& entry_deps = get_entry_dependencies(arg_dep[key]);
            LibFunctionInfo::LibArgDepInfo argDepInfo;
            if (entry_deps.dependency != DepInfo::UNKNOWN) {
                argDepInfo.dependency = entry_deps.dependency;
            } else {
                argDepInfo.argumentDependencies = std::move(entry_deps.argumentDependencies);
            }

            if (key == "return") {
                returnDeps = argDepInfo;
            } else {
                try {
                    int arg_num = std::stoi(key);
                    argDeps.insert(std::make_pair(arg_num, argDepInfo));
                } catch (const std::exception& e) {
                    llvm::dbgs() << "Invalid entry " << key << "\n";
                }
            }
        }
    }
    LibFunctionInfo libInfo(f_name,
                            std::move(argDeps),
                            returnDeps);
    m_libFunctionInfoProcessor(std::move(libInfo));
}

LibFunctionInfo::LibArgDepInfo LibraryInfoFromConfigFile::get_entry_dependencies(const Json::Value& entry)
{
    DepInfo::Dependency dep = DepInfo::UNKNOWN;
    std::unordered_set<int> arg_deps;
    for (const auto& val : entry) {
        if (val.isInt()) {
            arg_deps.insert(val.asInt());
        } else {
            const auto& val_str = val.asString();
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

}

