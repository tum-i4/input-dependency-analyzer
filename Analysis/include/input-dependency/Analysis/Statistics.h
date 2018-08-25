#pragma once

#include "input-dependency/Analysis/InputDependencyAnalysisPass.h"

#include <fstream>
#include <vector>

namespace llvm {
class Module;
}

namespace input_dependency {

class Statistics
{
public:
    // default format is JSON
    enum Format {
        TEXT,
        JSON
    };

public:
    class ReportWriter
    {
    public:
        struct key
        {
            std::string sectionName;
            std::string functionName;
            std::string statisticsTypeName;
            std::string valueName;
        };

    public:
        virtual ~ReportWriter()
        {}
        virtual void close();

        virtual void open(const std::string& file_name);
        virtual void write_entry(const key& k, double value) = 0;
        virtual void write_entry(const key& k, unsigned value) = 0;
        virtual void write_entry(const key& k, const std::string& value) = 0;
        virtual void write_entry(const key& k, const std::vector<std::string>& value) = 0;
        virtual void flush() = 0;

    protected:
        std::ofstream m_strm;
        std::string m_sectionName;
        std::string m_statsName;
    };

    using ReportWriterType = std::shared_ptr<ReportWriter>;

public:
    Statistics() = default;
    Statistics(const std::string& format, const std::string& file_name);
    Statistics(ReportWriterType writer);

    virtual ~Statistics()
    {
    }

public:
    ReportWriterType getReportWriter()
    {
        return m_writer;
    }

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
    void write_entry(const std::string& class_key, const std::string& key, double value);
    void write_entry(const std::string& class_key, const std::string& key, unsigned value);
    void write_entry(const std::string& class_key, const std::string& key, const std::string& value);
    void write_entry(const std::string& class_key, const std::string& key, const std::vector<std::string>& value);

protected:
    ReportWriterType m_writer;
    std::string m_sectionName;
    std::string m_statsTypeName;
};

}

