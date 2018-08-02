#pragma once

#include "PDG/PDGEdge.h"
#include "PDG/PDGNode.h"
#include "PDG/PDGLLVMNode.h"
#include "PDG/FunctionPDG.h"

#include "llvm/ADT/GraphTraits.h"
#include "llvm/Support/DOTGraphTraits.h"	// for dot graph traits
#include "llvm/ADT/STLExtras.h"			// for mapped_iter

using namespace pdg;

namespace llvm {

/*!
 * GraphTraits for nodes
 */
template<> struct GraphTraits<PDGNode*>
{
    typedef PDGNode NodeType;
    typedef PDGNode* NodeRef;
    typedef PDGNode::PDGEdgeType EdgeType;

    typedef std::pointer_to_unary_function<EdgeType, NodeRef> DerefEdge;
    typedef mapped_iterator<PDGNode::iterator, DerefEdge> ChildIteratorType;

    static NodeRef getEntryNode(NodeType* pdgN) {
        return pdgN;
    }

    static inline ChildIteratorType child_begin(NodeType* N) {
        return map_iterator(N->outEdgesBegin(), DerefEdge(edgeDereference));
    }
    static inline ChildIteratorType child_end(NodeType* N) {
        return map_iterator(N->outEdgesEnd(), DerefEdge(edgeDereference));
    }
    static NodeRef edgeDereference(EdgeType edge) {
        return edge->getDestination().get();
    }
};

/*!
 * Inverse GraphTraits for node which is used for inverse traversal.
 */
template<>
struct GraphTraits<Inverse<PDGNode*> >
{
    typedef PDGNode NodeType;
    typedef PDGNode* NodeRef;
    typedef PDGNode::PDGEdgeType EdgeType;

    typedef std::pointer_to_unary_function<EdgeType, NodeRef> DerefEdge;
    typedef mapped_iterator<PDGNode::iterator, DerefEdge> ChildIteratorType;

    static inline NodeRef getEntryNode(Inverse<NodeType* > G) {
        return G.Graph;
    }

    static inline ChildIteratorType child_begin(NodeType* N) {
        return map_iterator(N->inEdgesBegin(), DerefEdge(edgeDereference));
    }
    static inline ChildIteratorType child_end(NodeType* N) {
        return map_iterator(N->inEdgesEnd(), DerefEdge(edgeDereference));
    }
    static NodeRef edgeDereference(EdgeType edge) {
        return edge->getSource().get();
    }
};

/*!
 * GraphTraints
 */
template<> struct GraphTraits<FunctionPDG*> : public GraphTraits<PDGNode*> {

    static NodeRef getEntryNode(FunctionPDG* pdg) {
        return nullptr; // return null here, maybe later we could create a dummy node
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
// TODO: move to cpp file
template<>
struct DOTGraphTraits<FunctionPDG*> : public DefaultDOTGraphTraits
{
    typedef GraphTraits<FunctionPDG*>::NodeType NodeType;
    typedef GraphTraits<FunctionPDG*>::NodeRef NodeRef;
    typedef GraphTraits<FunctionPDG*>::EdgeType EdgeType;
    typedef GraphTraits<FunctionPDG*>::ChildIteratorType ChildIteratorType;

    DOTGraphTraits(bool isSimple = false)
        : DefaultDOTGraphTraits(isSimple)
    {
    }

    /// Return name of the graph
    static std::string getGraphName(FunctionPDG* graph) {
        return graph->getGraphName();
    }

    static std::string getNodeLabel(NodeRef node, FunctionPDG* graph)
    {
        return node->getNodeAsString();
    }

    static std::string getNodeAttributes(NodeRef node, FunctionPDG* graph)
    {
        if (llvm::isa<PDGLLVMBasicBlockNode>(node)) {
            return "color=black,style=dotted";
        } else if (llvm::isa<PDGLLVMNode>(node)) {
            return "color=black";
        }
        assert(false);
        return "";
    }

    static std::string getEdgeAttributes(NodeRef node, ChildIteratorType edge_iter, FunctionPDG* graph)
    {
        EdgeType edge = *(edge_iter.getCurrent());
        if (edge->isDataEdge()) {
            if (llvm::isa<pdg::PDGLLVMFormalArgumentNode>(edge->getDestination().get())) {
                return "color=green";
            } else {
                return "color=black";
            }
        } else if (edge->isControlEdge()) {
            return "color=blue";
        }
        assert(false);
        return "";
    }
};

}  // namespace llvm
