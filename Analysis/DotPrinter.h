#pragma once

#include "dot_interfaces.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

namespace dot {

class DotPrinter
{
public:
    using NodeType_ptr = std::shared_ptr<DotGraphNodeType>;

public:
    DotPrinter() = default;
    DotPrinter(const DotPrinter& ) = delete;
    DotPrinter& operator =(const DotPrinter& ) = delete;

public:
    void set_graph_name(const std::string& name)
    {
        graph_name = name;
    }

    void set_graph_label(const std::string& l)
    {
        label = l;
    }

    void print(const std::vector<NodeType_ptr>& nodes);

private:
    void write_header();
    void write_node(const NodeType_ptr& node);
    void write_node_connections(const NodeType_ptr& node);
    void write_footer();
    void finish();

    std::string create_header() const;
    std::string create_network_label() const;
    std::string create_node_label(const NodeType_ptr& node) const;
    std::string create_node_id(const NodeType_ptr& node) const;
    std::string create_edge_label(const std::string& node1_label, const NodeType_ptr& node) const;


private:
    std::string graph_name;
    std::string label;
    std::ofstream graph_stream;
};

}

