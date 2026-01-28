#pragma once

#include <iostream>
#include <string>
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
    std::list<Edge*> _fanins;
    std::list<Edge*> _fanouts;
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

};

class CNode {

  friend class Graph;

  private:
    bool _visited = false;
    tf::Task _task;
    std::list<Node*> _nodes;
    std::list<CEdge*> _fanins;
    std::list<CEdge*> _fanouts;

};

class CEdge {

  friend class Graph;

  private:
    CNode* _from;
    CNode* _to;

};

class Graph {

  public:
    // constructors
    Graph() {};
    Graph(const std::string& filename);

     // basic ops
    Node* insert_node(const std::string& name = "");
    Edge* insert_edge(Node* from, Node* to);

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

};

} // end of namespace pasta


























