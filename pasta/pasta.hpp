#pragma once

#include <iostream>
#include <string>
#include <list>
#include <random>
#include "taskflow/taskflow.hpp"
#include "wsq.hpp"

namespace pasta {

class Node;
class Edge;
class CNode;
class CEdge;
class Graph;

class Node {

  friend class Graph;

  public:
    Node(const std::string& name) : _name(name) {};

    inline size_t num_fanins() const {
      return _fanins.size();
    }

    inline size_t num_fanouts() const {
      return _fanouts.size();
    }

    inline std::string name() {
      return _name;
    }

  private:
    std::string _name;  

    /*
     * fanouts should not only include which fanout edges this node has(_fanouts)
     * but also the index of this edge in the fanin edge list of its fanout nodes(_fanout_satellites).
     * so when removing the node, we just need to traverse the _fanout_satellites once and erase the
     * edge of this node from the fanin edge list of fanout nodes.
     * similar method applied to fanins.
     */
    std::list<Edge*> _fanins;
    std::list<Edge*> _fanouts;
    std::list<std::pair<Node*, std::list<Edge*>::iterator>> _fanout_satellites;
    std::list<std::pair<Node*, std::list<Edge*>::iterator>> _fanin_satellites;

    std::list<Node>::iterator _node_satellite;
    std::list<CNode>::iterator _cnode_satellite;

    bool _visited = false;
    tf::Task _task;
    int _cluster_id = -1; // specify which partition (cluster) it belongs
    std::atomic<size_t> _dep_cnt{0};
    CNode* _cnode = NULL; // specify which cnode (cluster) it belongs

};

class Edge {

  friend class Graph;

  private:
    Node* _from;
    Node* _to;

    std::list<Edge*>::iterator _from_satellite; // edge satellite in from node _fanouts
    std::list<Edge*>::iterator _to_satellite; // edge satellite in to node _fanins

    std::list<Edge>::iterator _satellite;

};

class CNode {

  friend class Graph;

  private:
    bool _visited = false;
    tf::Task _task;
    std::list<Node*> _nodes;
    std::list<CEdge*> _fanins;
    std::list<CEdge*> _fanouts;
    std::list<CNode>::iterator _satellite;

};

class CEdge {

  friend class Graph;

  private:
    CNode* _from;
    CNode* _to;
    std::list<CEdge>::iterator _satellite;

};

class Graph {

  public:
    // constructors
    Graph() {};
    Graph(const std::string& filename);

     // basic ops
    Node* insert_node(const std::string& name = "");
    Edge* insert_edge(Node* from, Node* to);
    void remove_node(Node* node);
    void remove_edge(Edge* edge);

    // remove N nodes randomly
    void remove_random_nodes(size_t N, std::mt19937& gen);

    // remove N edges randomly
    void remove_random_edges(size_t N, std::mt19937& gen);

    // add N edges randomly
    size_t add_random_edges(size_t N, std::mt19937& gen, size_t max_tries_multiplier = 20); 

    // add N nodes randomly
    std::vector<Node*> add_random_nodes(size_t N, std::mt19937& gen, const std::string& name_prefix = "new");

    // helper
    inline size_t num_nodes() const {
      return _nodes.size();
    }
    inline size_t num_edges() const {
      return _edges.size();
    }
    inline void set_partition_size(const size_t partition_size) {
      _partition_size = partition_size;
    }
    void dump_graph();
    inline size_t get_incre_runtime_with_semaphore() {
      return incre_runtime_with_semaphore;
    } 
    // get topological order of current graph 
    void get_topo_order(std::vector<Node*>& topo); 

    // check cycle
    bool has_cycle_before_partition();
    bool has_cycle_after_partition();

    // C-PASTA
    void partition_c_pasta();

    // run graph with taskflow
    void run_graph_before_partition(size_t matrix_size);
    void run_graph_after_partition(size_t matrix_size);
    void run_graph_semaphore(size_t matrix_size, size_t num_semaphore);

  private:

    size_t _partition_size = 0;
    int _max_cluster_id = -1; // record the largest cluster id

    std::list<Node> _nodes;
    std::list<Edge> _edges;
    std::list<CNode> _cnodes;
    std::list<CEdge> _cedges;

    template <typename T>
    void _topo_dfs(std::vector<T*>& topo_order, T* node);

    void _assign_cluster_id(Node* node_ptr, std::vector<std::atomic<size_t>>& cluster_cnt, std::atomic<int>& max_cluster_id);

    void _build_partitioned_graph();

    // incremental update with semaphore runtime
    size_t incre_runtime_with_semaphore = 0;

};

} // end of namespace pasta


























