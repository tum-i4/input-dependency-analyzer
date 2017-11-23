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

    // Because can not have template virtual functions
    virtual void write_entry(const std::string& class_key, const std::string& key, unsigned value) = 0;
    virtual void write_entry(const std::string& class_key, const std::string& key, const std::string& value) = 0;
    virtual void write_entry(const std::string& class_key, const std::string& key, const std::vector<std::string>& value) = 0;
    virtual void flush() = 0;
};

class TextReportWriter : public Statistics::ReportWriter
{
public:
    TextReportWriter(const std::string& file_name);
    ~TextReportWriter();

    void write_entry(const std::string& class_key, const std::string& key, unsigned value) override;
    void write_entry(const std::string& class_key, const std::string& key, const std::string& value) override;
    void write_entry(const std::string& class_key, const std::string& key, const std::vector<std::string>& value) override;
    void flush() override
    {
        m_strm.flush();
    }

private:
    std::ofstream m_strm;
};

class JsonReportWriter : public Statistics::ReportWriter
{
public:
    JsonReportWriter(const std::string& file_name);
    ~JsonReportWriter();

    void write_entry(const std::string& class_key, const std::string& key, unsigned value) override
    {

        write(class_key, key, value);
    }

    void write_entry(const std::string& class_key, const std::string& key, const std::string& value) override
    {
        write(class_key, key, value);
    }

    void write_entry(const std::string& class_key, const std::string& key, const std::vector<std::string>& value) override
    {
        write(class_key, key, value);
    }

    void flush() override
    {
        m_strm << std::setw(4) << root;
    }

private:
    template<class ValueTy>
    void write(const std::string& class_key, const std::string& key, const ValueTy& value);

private:
    std::ofstream m_strm;
    json root;
};

JsonReportWriter::JsonReportWriter(const std::string& file_name)
{
    m_strm.open(file_name, std::ofstream::out);
}

JsonReportWriter::~JsonReportWriter()
{
    m_strm.close();
}

template <class ValueTy>
void JsonReportWriter::write(const std::string& class_key, const std::string& key, const ValueTy& value)
{
    root[class_key][key] = value;
    //m_strm << std::setw(4) << root;
}

TextReportWriter::TextReportWriter(const std::string& file_name)
{
    m_strm.open(file_name, std::ofstream::out);
}

TextReportWriter::~TextReportWriter()
{
    m_strm.close();
}

void TextReportWriter::write_entry(const std::string& class_key, const std::string& key, unsigned value)
{
    m_strm << class_key << " ";
    m_strm << key << " ";
    m_strm << value << "\n";
}

void TextReportWriter::write_entry(const std::string& class_key, const std::string& key, const std::string& value)
{
    m_strm << class_key << " ";
    m_strm << key << " ";
    m_strm << value << "\n";
}

void TextReportWriter::write_entry(const std::string& class_key, const std::string& key,
                                   const std::vector<std::string>& value)
{
    m_strm << class_key << " ";
    m_strm << key << " ";
    for (const auto& val : value) {
        m_strm << " " << val << "\n";
    }
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

void Statistics::write_entry(const std::string& class_key, const std::string& key, unsigned value)
{
    m_writer->write_entry(class_key, key, value);
}

void Statistics::write_entry(const std::string& class_key, const std::string& key, const std::string& value)
{
    m_writer->write_entry(class_key, key, value);
}

void Statistics::write_entry(const std::string& class_key, const std::string& key, const std::vector<std::string>& value)
{
    m_writer->write_entry(class_key, key, value);
}

void Statistics::flush()
{
    m_writer->flush();
}
}

