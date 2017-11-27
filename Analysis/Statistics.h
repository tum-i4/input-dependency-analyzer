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

    using Keys = std::vector<std::string>;

public:
    Statistics() = default;
    Statistics(const std::string& format, const std::string& file_name);

    virtual ~Statistics()
    {
    }

public:
    /// Set section name for this statistics in case it is a subsection of another statistics
    void setSectionName(const std::string& str)
    {
        m_sectionName = str;
    }

    void unsetSectionName()
    {
        m_sectionName.clear();
    }

    void setStatsTypeName(const std::string& str)
    {
        m_statsTypeName = str;
    }

    void unsetStatsTypeName()
    {
        m_statsTypeName.clear();
    }

    virtual void stop_report();
    virtual void resume_report(const std::string& file_name);
    virtual void flush();

    virtual void report() = 0;

protected:
    void write_entry(const std::string& class_key, const std::string& key, unsigned value);
    void write_entry(const std::string& class_key, const std::string& key, const std::string& value);
    void write_entry(const std::string& class_key, const std::string& key, const std::vector<std::string>& value);

protected:
    std::shared_ptr<ReportWriter> m_writer;
    std::string m_sectionName;
    std::string m_statsTypeName;
};

}

