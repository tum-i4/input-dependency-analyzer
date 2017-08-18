#pragma once

#include "DependencyAnaliser.h"
#include "DotPrinter.h"
#include "dot_interfaces.h"

#include "llvm/ADT/GraphTraits.h"

namespace input_dependency {

class value_dependence_graph
{
public:
    class node;
    using nodeT = std::shared_ptr<node>;
    using node_set = std::unordered_set<nodeT>;

    using nodep_set = std::unordered_set<node*>;
    using value_vector = std::vector<llvm::Value*>;

    class node
    {
    public:
        node() = default;
        node(llvm::Value* val);
        node(const value_vector& values);

        llvm::Value* get_value() const;
        const value_vector& get_values() const;
        bool is_compound() const;
        bool is_leaf() const;
        bool is_root() const;

        node_set& get_depends_on_values();
        node_set& get_dependent_values();
        bool add_depends_on_value(nodeT dep_node);
        bool add_dependent_value(nodeT dep_node);
        void remove_depends_on(nodeT dep_node);
        void remove_dependent_value(nodeT dep_node);
        void clear_dependent_values();
        void clear_depends_on_values();
        bool depends_on(nodeT n) const;

    public:
        using iterator = nodep_set::iterator;
        using const_iterator = nodep_set::const_iterator;

        iterator begin()
        {
            return depends_ons.begin();
        }
        const_iterator begin() const
        {
            return depends_ons.begin();
        }
        iterator end()
        {
            return depends_ons.end();
        }
        const_iterator end() const
        {
            return depends_ons.end();
        }

        iterator inv_begin()
        {
            return dependents.begin();
        }
        const_iterator inv_begin() const
        {
            return dependents.begin();
        }
        iterator inv_end()
        {
            return dependents.end();
        }
        const_iterator inv_end() const
        {
            return dependents.end();
        }

        size_t size() const
        {
            return depends_ons.size();
        }
    private:
     value_vector m_values;
     // values this node depends on
     node_set depends_on_values;
     node_set dependent_values;

     nodep_set depends_ons;
     nodep_set dependents;
    };

public:
    value_dependence_graph();
    void build(DependencyAnaliser::ValueDependencies& valueDeps,
               DependencyAnaliser::ValueDependencies& initialDeps);

    void dump(const std::string& name) const;

    nodeT get_root()
    {
        return root;
    }

    node_set& get_leaves()
    {
        return m_leaves;
    }

    node_set& get_input_deps()
    {
        return m_inputdeps;
    }

    node_set& get_input_indeps()
    {
        return m_inputindeps;
    }

private:
    void build_compound_nodes();

private:
    nodeT root;
    std::unordered_map<llvm::Value*, nodeT> nodes;
    node_set m_leaves;
    node_set m_inputdeps;
    node_set m_inputindeps;
};

class dot_node : public dot::DotGraphNodeType
{
public:
    using nodeT = value_dependence_graph::nodeT;

public:
    dot_node(const nodeT& n, DepInfo::Dependency dep = DepInfo::UNKNOWN);

public:
    std::vector<DotGraphNodeType_ptr> get_connections() const override;
    std::string get_id() const override;
    std::string get_label() const override;

private:
    nodeT m_node;
    DepInfo::Dependency m_dep;
};

}

namespace llvm {

/*
class scc_node
{
public:
    using nodeT = input_dependency::value_dependence_graph::nodeT;
    using iterator = std::vector<scc_node*>::iterator;
    using const_iterator = std::vector<scc_node*>::const_iterator;

public:
    scc_node(const nodeT& n)
        : m_node(n)
    {
        for (const auto& dep : m_node->get_depends_on_values()) {
            depends_on.push_back(new scc_node(dep));
        }
        for (const auto& dep : m_node->get_dependent_values()) {
            dependents.push_back(new scc_node(dep));
        }
    }

public:
    iterator begin()
    {
        return depends_on.begin();
    }
    const_iterator begin() const
    {
        return depends_on.begin();
    }
    iterator end()
    {
        return depends_on.end();
    }
    const_iterator end() const
    {
        return depends_on.end();
    }

    iterator inv_begin()
    {
        return dependents.begin();
    }
    const_iterator inv_begin() const
    {
        return dependents.begin();
    }
    iterator inv_end()
    {
        return dependents.end();
    }
    const_iterator inv_end() const
    {
        return dependents.end();
    }

    size_t size() const
    {
        return depends_on.size();
    }
    scc_node* getEntryBlock() const
    {
        return *depends_on.begin();
    }

    nodeT get_node() const
    {
        return m_node;
    }

private:
    nodeT m_node;
    std::vector<scc_node*> depends_on;
    std::vector<scc_node*> dependents;
};
*/

using scc_node = input_dependency::value_dependence_graph::node;

template <> struct GraphTraits<scc_node*>
{
    typedef scc_node NodeType;
    typedef scc_node *NodeRef;
    typedef input_dependency::value_dependence_graph::node::iterator ChildIteratorType;

    static NodeType* getEntryNode(scc_node* n) { return n; }


    static inline ChildIteratorType child_begin(NodeType *N) {
        return N->begin();
    }
    static inline ChildIteratorType child_end(NodeType *N) {
        return N->end();
    }
};

template <> struct GraphTraits<const scc_node*>
{
    typedef const scc_node NodeType;
    typedef const scc_node *NodeRef;
    typedef input_dependency::value_dependence_graph::node::const_iterator ChildIteratorType;

    static NodeType* getEntryNode(const scc_node* n) {return n; }

    static inline ChildIteratorType child_begin(NodeType *N) {
        return N->begin();
    }
    static inline ChildIteratorType child_end(NodeType *N) {
        return N->end();
    }
};

template <> struct GraphTraits<Inverse<scc_node*> >
{
    typedef scc_node NodeType;
    typedef scc_node *NodeRef;
    typedef input_dependency::value_dependence_graph::node::iterator ChildIteratorType;

    static NodeType* getEntryNode(Inverse<scc_node *> G) {return G.Graph; }

    static inline ChildIteratorType child_begin(NodeType *N) {
        return N->inv_begin();
    }
    static inline ChildIteratorType child_end(NodeType *N) {
        return N->inv_end();
    }
};

template <> struct GraphTraits<Inverse<const scc_node*> >
{
    typedef const scc_node NodeType;
    typedef const scc_node *NodeRef;
    typedef input_dependency::value_dependence_graph::node::const_iterator ChildIteratorType;

    static NodeType* getEntryNode(Inverse<const scc_node*> G) {return G.Graph; }

    static inline ChildIteratorType child_begin(NodeType *N) {
        return N->inv_begin();
    }
    static inline ChildIteratorType child_end(NodeType *N) {
        return N->inv_end();
    }

};
}

