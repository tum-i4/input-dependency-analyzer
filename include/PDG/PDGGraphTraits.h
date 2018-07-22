#pragma once

#include "PDG/PDGNode.h"
#include "PDG/FunctionPDG.h"

#include "llvm/ADT/GraphTraits.h"
#include "llvm/Support/DOTGraphTraits.h"	// for dot graph traits

using namespace pdg;

namespace llvm {

/*!
 * GraphTraits for nodes
 */
template<> struct GraphTraits<PDGNode>
{
    typedef PDGEdge::PDGNodeTy NodeType;
    typedef PDGNode::PDGEdgeType EdgeType;

    typedef PDGNode::PDGEdges::iterator ChildIteratorType;

    static NodeType* getEntryNode(NodeType* pdgN) {
        return pdgN;
    }

    static inline ChildIteratorType child_begin(const NodeType* N) {
        return N->outEdgesBegin();
    }
    static inline ChildIteratorType child_end(const NodeType* N) {
        return N->outEdgesEnd();
    }
};

/*!
 * Inverse GraphTraits for node which is used for inverse traversal.
 */
template<>
struct GraphTraits<Inverse<PDGNode> >
{
    typedef PDGEdge::PDGNodeTy NodeType;
    typedef PDGNode::PDGEdgeType EdgeType;

    typedef PDGNode::PDGEdges::iterator ChildIteratorType;

    static inline NodeType* getEntryNode(Inverse<NodeType* > G) {
        return G.Graph;
    }

    static inline ChildIteratorType child_begin(const NodeType* N) {
        return N.inEdgesBegin();
    }
    static inline ChildIteratorType child_end(const NodeType* N) {
        return N.inEdgesEnd();
    }
};

/*!
 * GraphTraints
 */
//template<class NodeTy,class EdgeTy> struct GraphTraits<GenericGraph<NodeTy,EdgeTy>* > : public GraphTraits<GenericNode<NodeTy,EdgeTy>*  > {
template<> struct GraphTraits<FunctionPDG> {
    typedef GenericGraph<NodeTy,EdgeTy> GenericGraphTy;
    typedef FunctionPDG::PDGNodeTy NodeType;

    static NodeType* getEntryNode(FunctionPDG* pdg) {
        return NULL; // return null here, maybe later we could create a dummy node
    }
    typedef FunctionPDG::iterator nodes_iterator;

    static nodes_iterator nodes_begin(FunctionPDG *G) {
        return G->nodesBegin();
    }
    static nodes_iterator nodes_end(FunctionPDG *G) {
        return G->nodesEnd();
    }

    static unsigned graphSize(FunctionPDG* G) {
        return G->size();
    }
};

/*!
 * Write value flow graph into dot file for debugging
 */
template<>
struct DOTGraphTraits<FunctionPDG*> : public DefaultDOTGraphTraits
{
    DOTGraphTraits(bool isSimple = false)
        : DefaultDOTGraphTraits(isSimple)
    {
    }

    /// Return name of the graph
    static std::string getGraphName(FunctionPDG* graph) {
        return graph->getGraphName();
    }

    static std::string getNodeLabel(FunctionPDG::PDGNodeTy node, FunctionPDG* graph)
    {
        return "nodeLabel";
    }

    static std::string getNodeAttributes(FunctionPDG::PDGNodeTy node, FunctionPDG* graph)
    {
        return "nodeAttributes";
    }

    static std::string getEdgeAttributes(FunctionPDG::PDGNodeTy node, PDGNode::iterator edge_iter, FunctionPDG* graph)
    {
        return "edgeAttributes";
    }

    static std::string getEdgeSourceLabel(FunctionPDG::PDGNodeTy node, PDGNode::iterator edge_iter)
    {
        return "edgeSourceLabel";
    }
};

}  // namespace llvm
