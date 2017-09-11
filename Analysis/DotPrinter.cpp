#include "DotPrinter.h"

namespace dot {

void DotPrinter::print(const std::vector<NodeType_ptr>& nodes)
{
    graph_stream.open(graph_name + ".dot");
    if (!graph_stream.is_open()) {
        std::cout << "Can not open file for writing the graph\n";
        return;
    }
    write_header();
    for (const auto& node : nodes) {
        write_node(node);
        write_node_connections(node);
    }
    write_footer();
    finish();
}

void DotPrinter::write_header()
{
    const std::string& header_label = create_header();
    graph_stream << header_label << std::endl;
}

void DotPrinter::write_node(const NodeType_ptr& node)
{
    const std::string& node_label = create_node_label(node);
    graph_stream << "\t" << node_label << std::endl;
}

void DotPrinter::write_node_connections(const NodeType_ptr& node)
{
    const auto& connections = node->get_connections();
    const std::string& node_id = create_node_id(node);
    for (const auto& conn_node : connections) {
        const std::string& conn_label = create_node_label(conn_node);
        graph_stream << "\t" << conn_label << std::endl;
        const std::string& connection = create_edge_label(node_id, conn_node);
        graph_stream << "\t" << connection << std::endl;
    }
}

void DotPrinter::write_footer()
{
    graph_stream << "}";
}

void DotPrinter::finish()
{
    graph_stream.close();
}

std::string DotPrinter::create_header() const
{
    std::ostringstream header;
    header << "digraph ";
    const auto& graph_label = create_network_label();
    header << graph_label;
    header << " {\n";
    header << "\t";
    header << "label=";
    header << graph_label;
    header << ";\n";
    return header.str();
}

std::string DotPrinter::create_network_label() const
{
    return std::string("\"checkers network \'" + graph_name + "\'\"");
}

std::string DotPrinter::create_node_label(const NodeType_ptr& node) const
{
    // nodeid [shape=record, label="function name  nodeid"];
    const std::string& function_name = node->get_label();

    std::ostringstream node_label;
    node_label << create_node_id(node);
    node_label << " [shape=record,label=\"{";
    node_label << function_name;
    node_label << "}\"";
    node_label << "];";
    return node_label.str();
}

std::string DotPrinter::create_node_id(const NodeType_ptr& node) const
{
    std::ostringstream id;
    id << "Node" << node->get_id();
    return id.str();
}

std::string DotPrinter::create_edge_label(const std::string& node1_label,
                                          const NodeType_ptr& conn_node) const
{
    std::ostringstream label;
    label << node1_label << " -> ";
    label << create_node_id(conn_node);
    return label.str();
}

}

