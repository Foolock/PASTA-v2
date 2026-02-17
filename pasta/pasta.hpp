#pragma once

#include <iostream>
#include <string>
#include <list>
#include <random>
#include "taskflow/taskflow.hpp"
#include "wsq.hpp"

namespace pasta {

enum class RunMode {
  None,
  Semaphore,
  IncrementalPartition,
  Partition
};

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

    bool _have_acquired_semaphore = false;

    int _id = -1;

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

    // used in cudaflow reconstructed graph
    int _topo_id = -1; // idx in topological order
    int _level = -1;
    int _lid = -1; // indicate its index within its level 
    int _sm = -1;
    std::vector<Node*> _reconstructed_fanins;
    std::vector<Node*> _reconstructed_fanouts;

    // Incremental cudaflow partitioning:
    // Use cudaflow partitioning to add
    // just one extra fanin/fanout to limit the maximum parallelism
    // the other dependencies follow the original graph
    Node* _extra_fanin;
    Node* _extra_fanout;

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
    Node* insert_node(const std::string& name = "", RunMode mode = RunMode::None, size_t matrix_size = 8);
    Edge* insert_edge(Node* from, Node* to, RunMode mode = RunMode::None);
    void remove_node(Node* node, RunMode mode = RunMode::None);
    void remove_edge(Edge* edge, RunMode mode = RunMode::None);

    // remove N nodes randomly
    void remove_random_nodes(size_t N, std::mt19937& gen, RunMode mode = RunMode::None);

    // remove N edges randomly
    void remove_random_edges(size_t N, std::mt19937& gen, RunMode mode = RunMode::None);

    // add N edges randomly
    size_t add_random_edges(size_t N, std::mt19937& gen, size_t max_tries_multiplier = 20, RunMode mode = RunMode::None); 

    // add N nodes randomly
    std::vector<Node*> add_random_nodes(size_t N, std::mt19937& gen, 
                                        const std::string& name_prefix = "new", 
                                        RunMode mode = RunMode::None, size_t matrix_size = 8);

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
    inline size_t get_incre_runtime_with_semaphore() const {
      return _incre_runtime_with_semaphore;
    } 
    inline size_t get_incre_runtime_with_semaphore_graph_construct() const {
      return _incre_runtime_with_semaphore_graph_construct;
    } 
    inline size_t get_incre_runtime_with_cudaflow_partition() const {
      return _incre_runtime_with_cudaflow_partition;
    }
    inline size_t get_incre_partition_runtime_with_cudaflow_partition() const {
      return _incre_partition_runtime_with_cudaflow_partition;
    }
    inline size_t get_incre_construct_runtime_with_cudaflow() const {
      return _incre_construct_runtime_with_cudaflow;
    }
    void test_func();

    // check cycle
    bool has_cycle_before_partition();
    bool has_cycle_after_partition();

    // C-PASTA
    void partition_c_pasta();

    // CUDAFlow partition
    // reconstruct graph based on cudaflow
    void partition_cudaflow(size_t num_streams = 4);

    // Incremental CUDAFlow partition
    // just add one extra fanin/fanout 
    void partition_cudaflow_incremental(size_t num_streams = 4);

    // check if two DAGs that shares same set of vertices, 
    // one partitioned by cudaflow, one original, share at least one topological order
    // we just need to check if the union graph of G1 and G2 is acyclic
    // if it is, then they share at least one topological order
    // union graph is "same set of vertices built on all the edges in G1 and G2"
    bool is_cudaflow_partition_share_same_topo_order();
    // checker for incremental cudaflow partitioning
    bool is_incre_cudaflow_partition_share_same_topo_order();

    // run graph with taskflow
    void run_graph_before_partition(size_t matrix_size);
    void run_graph_after_partition(size_t matrix_size);
    void run_graph_semaphore(size_t matrix_size, size_t num_semaphore); // num_semaphore = max_parallelism
    void run_graph_cudaflow_partition(size_t matrix_size, size_t num_streams); // num_streams = max_parallelism
    void run_graph_cudaflow_partition_incremental(size_t matrix_size, size_t num_streams); // num_streams = max_parallelism

  private:

    size_t _partition_size = 0;
    int _max_cluster_id = -1; // record the largest cluster id

    std::list<Node> _nodes;
    std::list<Edge> _edges;
    std::list<CNode> _cnodes;
    std::list<CEdge> _cedges;

    // get level list of current graph 
    std::vector<std::vector<Node*>> _get_level_list();

    // get topological order of current graph using BFS
    std::vector<Node*> _get_topo_order_bfs();

    // get reversed topological order of current graph using DFS 
    void _get_topo_reverse_order_dfs(std::vector<Node*>& topo); 

    template <typename T>
    void _topo_dfs(std::vector<T*>& topo_order, T* node);

    void _assign_cluster_id(Node* node_ptr, std::vector<std::atomic<size_t>>& cluster_cnt, std::atomic<int>& max_cluster_id);

    void _build_partitioned_graph();

    // incremental update with semaphore runtime
    size_t _incre_runtime_with_semaphore = 0;
    size_t _incre_runtime_with_semaphore_graph_construct = 0;

    // incremental update with cudaflow_partition runtime
    size_t _incre_runtime_with_cudaflow_partition = 0;
    size_t _incre_partition_runtime_with_cudaflow_partition = 0;
    size_t _incre_construct_runtime_with_cudaflow = 0;

    tf::Taskflow _taskflow;
    tf::Executor _executor{std::thread::hardware_concurrency()};
    tf::Semaphore _semaphore{std::thread::hardware_concurrency()};  
    bool _first_run = true;

};

} // end of namespace pasta


























