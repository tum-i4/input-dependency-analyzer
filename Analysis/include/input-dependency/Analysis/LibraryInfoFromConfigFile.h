#pragma once

#include "input-dependency/Analysis/LibraryInfoCollector.h"
#include "nlohmann/json.hpp"

namespace input_dependency {

class LibraryInfoFromConfigFile : public  LibraryInfoCollector
{
private:
    using json = nlohmann::json;

public:
    LibraryInfoFromConfigFile(const LibraryInfoCallback& callback,
                              const std::string& config_file)
        : LibraryInfoCollector(callback)
        , m_config_file(config_file)
    {
    }
    
public:
    void setup() override;

private:
    void add_library_function(const json& function_value);
    void parse_dependencies(LibFunctionInfo& libInfo, const json& arg_deps);
    LibFunctionInfo::LibArgDepInfo get_entry_dependencies(const json& entry);
    LibFunctionInfo::ArgumentIndices get_callback_indices(const json& entry);

private:
    const std::string& m_config_file;
}; // class LibraryInfoFromConfigFile

}

