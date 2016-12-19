//
//  Graph.h
//  
//
//  Created by Mohsen Ahmadvand on 19/12/16.
//
//

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include <vector>
#include <algorithm>
using namespace llvm;

namespace {
    class Graph{
    public:
        Graph(int V){
            this->V=V;
        }
        
        void addEdge(int v, int w){
            adj[v].push_back(w);
            adj[w].push_back(v);  // Note: the graph is undirected
        }
        
        int addnewBB(BasicBlock& B){
            for (std::map<int, BasicBlock*>::iterator it = bbmap.begin(); it != bbmap.end(); ++it )
                if (it->second == &B)
                    return it->first;
            int size=bbmap.size();
            bbmap[size]=&B;
            V=bbmap.size();
            return size;
        }
        
        std::vector<int> AP(){
            // Mark all the vertices as not visited
            bool visited[V];
            int disc[V];
            int low[V];
            int parent[V];
            bool ap[V]; // To store articulation points
            
            // Initialize parent and visited, and ap(articulation point) arrays
            for (int i = 0; i < V; i++)
            {
                parent[i] = -1;
                visited[i] = false;
                ap[i] = false;
            }
            
            // Call the recursive helper function to find articulation points
            // in DFS tree rooted with vertex 'i'
            for (int i = 0; i < V; i++)
                if (visited[i] == false)
                    APUtil(i, visited, disc, low, parent, ap);
            
            std::vector<int> ids={};
            // Now ap[] contains articulation points, print them
            for (int i = 0; i < V; i++){
                if (ap[i] == true){
                    ids.push_back(i);
                    errs() << i << "\n";
                }
            }
            return ids;
            
            
        }
        
        void APUtil(int u, bool visited[], int disc[], int low[],
                    int parent[], bool ap[]){
            // A static variable is used for simplicity, we can avoid use of static
            // variable by passing a pointer.
            static int time = 0;
            
            // Count of children in DFS Tree
            int children = 0;
            
            // Mark the current node as visited
            visited[u] = true;
            
            // Initialize discovery time and low value
            disc[u] = low[u] = ++time;
            
            // Go through all vertices aadjacent to this
            std::vector<int>::iterator i;
            for (i = adj[u].begin(); i != adj[u].end(); ++i)
            {
                int v = *i;  // v is current adjacent of u
                
                // If v is not visited yet, then make it a child of u
                // in DFS tree and recur for it
                if (!visited[v])
                {
                    children++;
                    parent[v] = u;
                    APUtil(v, visited, disc, low, parent, ap);
                    
                    // Check if the subtree rooted with v has a connection to
                    // one of the ancestors of u
                    low[u]  = std::min(low[u], low[v]);
                    
                    // u is an articulation point in following cases
                    
                    // (1) u is root of DFS tree and has two or more chilren.
                    if (parent[u] == -1 && children > 1)
                        ap[u] = true;
                    
                    // (2) If u is not root and low value of one of its child is more
                    // than discovery value of u.
                    if (parent[u] != -1 && low[v] >= disc[u])
                        ap[u] = true;
                }
                
                // Update low value of u for parent function calls.
                else if (v != parent[u])
                    low[u]  = std::min(low[u], disc[v]);
            }
        }
    private:
        int V=0;
        std::map<int,std::vector<int>> adj;
        std::map<int, BasicBlock*> bbmap;
    };
}
