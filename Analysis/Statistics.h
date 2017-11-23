#pragma once

#include "InputDependencyAnalysis.h"

#include <vector>

namespace llvm {
class Module;
}

namespace input_dependency {

class Statistics
{
public:
    class ReportWriter;
    // default format is JSON
    enum Format {
        TEXT,
        JSON
    };

public:
    Statistics() = default;
    Statistics(const std::string& format, const std::string& file_name);

    virtual ~Statistics()
    {
    }

public:
    virtual void report() = 0;

protected:
    void write_entry(const std::string& class_key, const std::string& key, unsigned value);
    void write_entry(const std::string& class_key, const std::string& key, const std::string& value);
    void write_entry(const std::string& class_key, const std::string& key, const std::vector<std::string>& value);
    void flush();

protected:
    std::shared_ptr<ReportWriter> m_writer;
};

}

