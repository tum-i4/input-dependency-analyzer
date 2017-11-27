#include "Statistics.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"

#include "json/json.hpp"
#include <fstream>

using json = nlohmann::json;

namespace input_dependency {

class Statistics::ReportWriter
{
public:
    struct key
    {
        std::string sectionName;
        std::string functionName;
        std::string statisticsTypeName;
        std::string valueName;
    };

    struct clone_data
    {
        std::string name;
        unsigned numOfClonnedInst;
        unsigned numOfInstAfterCloning;
        unsigned numOfInDepInstAfterCloning;
        std::vector<std::string> clonnedFuncs;
    };

    struct extraction_data
    {
        std::string name;
        unsigned numOfExtractedInst;
        unsigned numOfMediateInst;
        std::vector<std::string> extractedFuncs;
    };

public:
    virtual ~ReportWriter()
    {
    }

    virtual void close()
    {
        m_strm.close();
        m_strm.clear();
    }

    virtual void open(const std::string& file_name)
    {
        if (!m_strm.is_open()) {
            m_strm.open(file_name, std::ofstream::app);
        }
    }

    // Because can not have template virtual functions
    virtual void write_entry(const key& k, unsigned value) = 0;
    virtual void write_entry(const key& k, const std::string& value) = 0;
    virtual void write_entry(const key& k, const std::vector<std::string>& value) = 0;
    virtual void flush() = 0;

protected:
    std::ofstream m_strm;
    std::string m_sectionName;
    std::string m_statsName;
};

class TextReportWriter : public Statistics::ReportWriter
{
public:
    TextReportWriter(const std::string& file_name);
    ~TextReportWriter();

    void write_entry(const key& k, unsigned value) override;
    void write_entry(const key& k, const std::string& value) override;
    void write_entry(const key& k, const std::vector<std::string>& value) override;
    void flush() override
    {
        m_strm.flush();
    }

private:
    void write_key(const key& k);
};

class JsonReportWriter : public Statistics::ReportWriter
{
public:
    JsonReportWriter(const std::string& file_name);
    ~JsonReportWriter();

    void write_entry(const key& k, unsigned value) override
    {

        write(k, value);
    }

    void write_entry(const key& k, const std::string& value) override
    {
        write(k, value);
    }

    void write_entry(const key& k, const std::vector<std::string>& value) override
    {
        write(k, value);
    }

    void flush() override
    {
        m_strm << std::setw(4) << root;
        root.clear();
    }

private:
    template<class ValueTy>
    void write(const key& k, const ValueTy& value);

private:
    json root;
};

JsonReportWriter::JsonReportWriter(const std::string& file_name)
{
    m_strm.open(file_name, std::ofstream::app);
}

JsonReportWriter::~JsonReportWriter()
{
    m_strm.close();
}

template <class ValueTy>
void JsonReportWriter::write(const key& k, const ValueTy& value)
{
    if (!k.sectionName.empty()) {
        if (!k.statisticsTypeName.empty()) {
            root[k.sectionName][k.functionName][k.statisticsTypeName][k.valueName] = value;
        } else {
            root[k.sectionName][k.functionName][k.valueName] = value;
        }
    } else {
        if (!k.statisticsTypeName.empty()) {
            root[k.functionName][k.statisticsTypeName][k.valueName] = value;
        } else {
            root[k.functionName][k.valueName] = value;
        }
    }
}

TextReportWriter::TextReportWriter(const std::string& file_name)
{
    m_strm.open(file_name, std::ofstream::app);
}

TextReportWriter::~TextReportWriter()
{
    m_strm.close();
}

void TextReportWriter::write_entry(const key& k, unsigned value)
{
    write_key(k);
    m_strm << value << "\n";
}

void TextReportWriter::write_entry(const key& k, const std::string& value)
{
    write_key(k);
    m_strm << value << "\n";
}

void TextReportWriter::write_entry(const key& k,
                                   const std::vector<std::string>& value)
{
    write_key(k);
    for (const auto& val : value) {
        m_strm << " " << val << "\n";
    }
}

void TextReportWriter::write_key(const key& k)
{
    if (!k.sectionName.empty()) {
        m_strm << k.sectionName << " ";
    }
    m_strm << k.functionName << " ";
    if (!k.statisticsTypeName.empty()) {
        m_strm << k.statisticsTypeName << " ";
    }
    m_strm << k.valueName << " ";
}

Statistics::Format string_to_stats_format(const std::string& stats_format)
{
    if (stats_format == "text") {
        return Statistics::TEXT;
    }
    return Statistics::JSON;
}

Statistics::Statistics(const std::string& format_str,
                       const std::string& file_name)
{
    Format format = string_to_stats_format(format_str);
    switch (format) {
    case TEXT:
        m_writer.reset(new TextReportWriter(file_name));
        break;
    case JSON:
        m_writer.reset(new JsonReportWriter(file_name));
        break;
    }
}

void Statistics::stop_report()
{
    m_writer->close();
}

void Statistics::resume_report(const std::string& file_name)
{
    m_writer->open(file_name);
}

void Statistics::write_entry(const std::string& class_key, const std::string& key, unsigned value)
{
    m_writer->write_entry(ReportWriter::key{m_sectionName, class_key, m_statsTypeName, key}, value);
}

void Statistics::write_entry(const std::string& class_key, const std::string& key, const std::string& value)
{
    m_writer->write_entry(ReportWriter::key{m_sectionName, class_key, m_statsTypeName, key}, value);
}

void Statistics::write_entry(const std::string& class_key, const std::string& key, const std::vector<std::string>& value)
{
    m_writer->write_entry(ReportWriter::key{m_sectionName, class_key, m_statsTypeName, key}, value);
}

void Statistics::flush()
{
    m_writer->flush();
}
}

