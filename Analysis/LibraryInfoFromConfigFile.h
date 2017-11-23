#pragma once

#include "LibraryInfoCollector.h"
#include "json/json.hpp"

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
    LibFunctionInfo::LibArgDepInfo get_entry_dependencies(const json& entry);

private:
    const std::string& m_config_file;
}; // class LibraryInfoFromConfigFile

}

