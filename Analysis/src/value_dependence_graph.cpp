#include "input-dependency/Analysis/value_dependence_graph.h"

#include "llvm/ADT/SCCIterator.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <list>
#include <sstream>

namespace input_dependency {

value_dependence_graph::node::node(llvm::Value* val)
    : m_values(1, val)
{
}

value_dependence_graph::node::node(const value_vector& vals)
    : m_values(vals)
{
}

llvm::Value* value_dependence_graph::node::get_value() const
{
    if (m_values.empty()) {
        return nullptr;
    }
    return m_values.front();
}

const value_dependence_graph::value_vector& value_dependence_graph::node::get_values() const
{
    return m_values;
}

bool value_dependence_graph::node::is_compound() const
{
    return m_values.size() > 1;
}

bool value_dependence_graph::node::is_leaf() const
{
    return depends_on_values.empty();
}

bool value_dependence_graph::node::is_root() const
{
    return m_values.empty();
}


value_dependence_graph::node_set& value_dependence_graph::node::get_depends_on_values()
{
    return depends_on_values;
}

value_dependence_graph::node_set& value_dependence_graph::node::get_dependent_values()
{
    return dependent_values;
}

void value_dependence_graph::node::add_depends_on_value(value_dependence_graph::nodeT dep_node)
{
    depends_on_values.insert(dep_node).second;
    depends_ons.insert(dep_node.get());
}

void value_dependence_graph::node::add_dependent_value(value_dependence_graph::nodeT dep_node)
{
    dependent_values.insert(dep_node).second;
    dependents.insert(dep_node.get());
}

void value_dependence_graph::node::remove_depends_on(value_dependence_graph::nodeT dep_node)
{
    depends_ons.erase(dep_node.get());
    depends_on_values.erase(dep_node);
}

void value_dependence_graph::node::remove_dependent_value(value_dependence_graph::nodeT dep_node)
{
    dependent_values.erase(dep_node);
}

void value_dependence_graph::node::clear_dependent_values()
{
    dependent_values.clear();
}

void value_dependence_graph::node::clear_depends_on_values()
{
    depends_on_values.clear();
}

bool value_dependence_graph::node::depends_on(value_dependence_graph::nodeT n) const
{
    if (depends_on_values.empty()) {
        return false;
    }
    return depends_on_values.find(n) != depends_on_values.end();
}

value_dependence_graph::value_dependence_graph()
    : root(new node())
{
}

void value_dependence_graph::build(DependencyAnaliser::ValueDependencies& valueDeps,
                                   DependencyAnaliser::ValueDependencies& initialDeps)
{
    //std::unordered_map<llvm::Value*, nodeT> nodes;
    std::list<llvm::Value*> processing_list;
    std::unordered_set<llvm::Value*> processed_values;

    for (auto& val : valueDeps) {
        processing_list.push_back(val.first);
    }
    while (!processing_list.empty()) {
        auto process_val = processing_list.back();
        processing_list.pop_back();
        if (!processed_values.insert(process_val).second) {
            continue;
        }
        auto item = valueDeps.find(process_val);
        if (item == valueDeps.end()) {
            item = initialDeps.find(process_val);
            if (item == initialDeps.end()) {
                continue;
            }
            valueDeps[process_val] = item->second;
            item = valueDeps.find(process_val);
        }
        auto item_dep = item->second.getValueDep();
        auto res = nodes.insert(std::make_pair(item->first, nodeT(new node(item->first))));
        nodeT& item_node = res.first->second;

        root->add_depends_on_value(item_node);
        item_node->add_dependent_value(root);

        if (!item_dep.isValueDep()) {
            if (item_dep.isInputDep()) {
                m_inputdeps.insert(item_node);
            } else if (item_dep.isInputIndep()) {
                m_inputindeps.insert(item_node);
            }
            m_leaves.insert(item_node);
            continue;
        }

        auto& value_deps = item_dep.getValueDependencies();
        std::vector<llvm::Value*> values_to_erase;
        for (auto& val : value_deps) {
            if (val == item->first) {
                values_to_erase.push_back(item->first);
                continue;
            }
            auto val_res = nodes.insert(std::make_pair(val, nodeT(new node(val))));
            auto& dep_node = val_res.first->second;
            bool is_global = llvm::dyn_cast<llvm::GlobalVariable>(val);
            if (is_global && valueDeps.find(val) == valueDeps.end()) {
                continue;
            }
            item_node->add_depends_on_value(dep_node);
            dep_node->add_dependent_value(item_node);
            // is not in value list modified or referenced in this block
            if (valueDeps.find(val) == valueDeps.end() && processed_values.find(val) == processed_values.end()) {
                processing_list.push_back(val);
            }
        }
        if (item_node->is_leaf()) {
            m_leaves.insert(item_node);
        }
    }
    build_compound_nodes();
}

void value_dependence_graph::build_compound_nodes()
{
    using values_scc_iter = llvm::scc_iterator<llvm::scc_node*>;
    values_scc_iter it = values_scc_iter::begin(root.get());
    node_set remove_nodes;
    while (it != values_scc_iter::end(root.get())) {
        auto& scc_nodes = *it;
        if (scc_nodes.size() == 1) {
            ++it;
            continue;
        }
        std::unordered_set<llvm::Value*> scc_values;
        for (auto& node : scc_nodes) {
            // TODO: skip if is in removed_nodes?
            scc_values.insert(node->get_value());
        }
        nodeT comp_node(new node(value_vector(scc_values.begin(), scc_values.end())));
        comp_node->add_dependent_value(root);
        for (auto& val : scc_values) {
            auto val_node = nodes.find(val);
            assert(val_node != nodes.end());
            remove_nodes.insert(val_node->second);
            for (auto& dep_on : val_node->second->get_depends_on_values()) {
                // TODO: check for the case where dep_on is a compound node itself
                if (scc_values.find(dep_on->get_value()) != scc_values.end()) {
                    continue;
                }
                comp_node->add_depends_on_value(dep_on);
                // why not do this in the same function?
                dep_on->add_dependent_value(comp_node);
            }
            for (auto& dep : val_node->second->get_dependent_values()) {
                if (scc_values.find(dep->get_value()) != scc_values.end()) {
                    continue;
                }
                comp_node->add_dependent_value(dep);
                dep->add_depends_on_value(comp_node);
            }
            root->add_depends_on_value(comp_node);
            nodes.erase(val); // to delete shared_ptr
            nodes[val] = comp_node;
        }
        if (comp_node->is_leaf()) {
            m_leaves.insert(comp_node);
        }
        ++it;
    }
    for (auto remove_node : remove_nodes) {
        for (auto& dep_on : remove_node->get_depends_on_values()) {
            dep_on->remove_dependent_value(remove_node);
        }
        for (auto& dep : remove_node->get_dependent_values()) {
            dep->remove_depends_on(remove_node);
        }
        root->remove_dependent_value(remove_node);
        remove_node.reset();
    }
    remove_nodes.clear();
}

void value_dependence_graph::dump(const std::string& name) const
{

    using GraphNodeType_ptr = std::shared_ptr<dot::DotGraphNodeType>;
    std::vector<GraphNodeType_ptr> graph_nodes;
    for (const auto& n : nodes) {
        graph_nodes.push_back(GraphNodeType_ptr(new dot_node(n.second, DepInfo::INPUT_DEP)));
    } 
    //for (const auto& n : m_inputindeps) {
    //    graph_nodes.push_back(GraphNodeType_ptr(new dot_node(n, DepInfo::INPUT_DEP)));
    //} 
    dot::DotPrinter printer;
    printer.set_graph_name(name);
    printer.set_graph_label("Value dependency graph");
    printer.print(graph_nodes);
}

dot_node::dot_node(const nodeT& n, DepInfo::Dependency dep)
    : m_node(n)
    , m_dep(dep)
{
}

std::vector<dot_node::DotGraphNodeType_ptr> dot_node::get_connections() const
{
    std::vector<DotGraphNodeType_ptr> connections;
    for (const auto& dep : m_node->get_depends_on_values()) {
        connections.push_back(DotGraphNodeType_ptr(new dot_node(dep)));
    }
    return connections;
}

std::string dot_node::get_id() const
{
    std::stringstream ss;
    auto val = m_node->get_value();
    if (val == nullptr) {
        ss << std::to_string(0);
    } else {
        ss << m_node->get_value();  
    }
    return ss.str();
}

std::string dot_node::get_label() const
{
    std::string str("");
    llvm::raw_string_ostream str_strm(str);
    if (m_node->get_value() == nullptr) {
        str_strm << "root\n";
    } else if (m_node->is_compound()) {
        str_strm << "* ";
        for (const auto& val : m_node->get_values()) {
            str_strm << val->getName() << "     ";
        }
    } else {
        str_strm << *m_node->get_value();
    }
    if (m_dep == DepInfo::INPUT_DEP) {
        str_strm << " DEP";
    } else if (m_dep == DepInfo::INPUT_INDEP) {
        str_strm << " INDEP";
    }
    return str_strm.str();
}


}

