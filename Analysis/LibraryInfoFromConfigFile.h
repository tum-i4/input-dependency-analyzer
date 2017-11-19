#pragma once

#include "LibraryInfoCollector.h"

namespace Json {
class Value;
}

namespace input_dependency {

class LibraryInfoFromConfigFile : public  LibraryInfoCollector
{
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
    void add_library_function(const Json::Value& function_value);
    LibFunctionInfo::LibArgDepInfo get_entry_dependencies(const Json::Value& entry);

private:
    const std::string& m_config_file;
}; // class LibraryInfoFromConfigFile

}

