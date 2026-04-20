#pragma once

#include <iostream>
#include <string>
#include <list>
#include <random>
#include <set>
#include "taskflow/taskflow.hpp"
#include "wsq.hpp"
#include <utility>
#include <unordered_set>

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

    // pointer in _topo_nodes
    std::list<Node*>::iterator _topo_it;
    
    // indicate its position in the topological sequence
    uint64_t _pos = 0;

    uint64_t _dfs_tag = 0;

    // Indicate the edge between this node and its next node in _topo_nodes exists in the original graph
    Node* _linked_to = nullptr; // linked_to is used to check if Taskflow has the same topological order as my graph
    bool _linked_to_is_actual = false;
    std::set<Node*> _fanout_set; // to quickly identify if v is a fanout of u

    size_t _num_fanins = 0;

    /* Indicate the node location within one level in _level_list */
    std::list<Node*>::iterator _level_it;

    bool _in_topo_nodes = false;  // true iff this node is linked into _topo_nodes

};

class Edge {

  friend class Graph;

  private:
    Node* _from;
    Node* _to;

    std::list<Edge*>::iterator _from_satellite; // edge satellite in from node _fanouts
    std::list<Edge*>::iterator _to_satellite; // edge satellite in to node _fanins

    std::list<Edge>::iterator _satellite;

    bool _is_limit_parallelism = false;

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
    Graph(const std::string& filename, RunMode mode = RunMode::None, size_t matrix_size = 8);

    // basic ops
    Node* insert_node(const std::string& name = "", RunMode mode = RunMode::None, size_t matrix_size = 8);
    // if is_limit_parallelism = true, this edge is used to limit parallelism and thus cannot be removed
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
    inline size_t get_process_backward_edge_time() const {
      return _process_backward_edge_time;
    }
    inline size_t get_generate_topo_order_time() const {
      return _generate_topo_order_time;
    }
    inline size_t get_num_backward_edges() const {
      return _num_backward_edges;
    }
    inline size_t get_pasta_taskflow_runtime() const {
      return _pasta_taskflow_runtime;
    }
    inline size_t get_critical_path_length_original() const {
      return _critical_path_length_original;
    }
    inline size_t get_critical_path_length_constrained() const {
      return _critical_path_length_constrained;
    }
    inline size_t get_cudaflow_partitioning_runtime() const {
      return _cudaflow_partitioning_runtime;
    }
    inline size_t get_cudaflow_rebuild_level_list_runtime() const {
      return _cudaflow_rebuild_level_list_runtime;
    }
    inline size_t get_cudaflow_incre_level_list_runtime() const {
      return _cudaflow_incre_level_list_runtime;
    }
    inline size_t get_cudaflow_assign_streams_runtime() const {
      return _cudaflow_assign_streams_runtime;
    }
    inline size_t get_pasta_partitioning_runtime() const {
      return _pasta_partitioning_runtime;
    }
    inline size_t get_cudaflow_taskflow_buildtime() const {
      return _cudaflow_taskflow_buildtime;
    }
    inline size_t get_pasta_taskflow_buildtime() const {
      return _pasta_taskflow_buildtime;
    }
    inline size_t get_num_breakable_nodes() const {
      return _num_breakable_nodes;
    }
    inline size_t get_num_selected_breakable_nodes() const {
      return _num_selected_breakable_nodes;
    }
    inline size_t get_semaphore_taskflow_runtime() const {
      return _semaphore_taskflow_runtime;
    }
    inline size_t get_semaphore_taskflow_buildtime() const {
      return _semaphore_taskflow_buildtime;
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

    // check if two DAGs that shares same set of vertices, 
    // one partitioned by cudaflow, one original, share at least one topological order
    // we just need to check if the union graph of G1 and G2 is acyclic
    // if it is, then they share at least one topological order
    // union graph is "same set of vertices built on all the edges in G1 and G2"
    bool is_cudaflow_partition_share_same_topo_order();

    // run graph with taskflow
    void run_graph_before_partition(size_t matrix_size);
    void run_graph_after_partition(size_t matrix_size);
    void run_graph_semaphore(size_t num_semaphore); // num_semaphore = max_parallelism
    void run_graph_cudaflow_partition(size_t matrix_size, size_t num_streams); // num_streams = max_parallelism
    /* For this version of cudaflow, we try to improve the runtime of assigning streams first, then see 
       try to incrementally update level list, then incrementally apply changes to Taskflow   
    */
    void run_graph_cudaflow_partition_update(size_t num_streams); // num_streams = max_parallelism
    /* Now apply incremental update for level list */
    void run_graph_cudaflow_partition_update_incre(size_t num_streams); // num_streams = max_parallelism
    // cur_parallelism is the parallelism limit for current iteration
    // max_parallelism is the maximum potential parallelism
    void run_graph_pasta_partition(size_t matrix_size, size_t cur_parallelism, size_t max_parallelism);  
    void run_graph_pasta_partition_seq(size_t matrix_size, size_t cur_parallelism, size_t max_parallelism);  
    // for this version of pasta, we redo DFS in each iteration to obtain better taskflow runtime (shorten critical path)
    // ctest case not ready
    void run_graph_pasta_partition_full(size_t matrix_size, size_t cur_parallelism, size_t max_parallelism);  
    /* For this version of pasta, we incrementally maintain a topological order and assign nodes to streams in a round robin fashion.
       We need to generate _topo_nodes using BFS in this version.
    */
    void run_graph_pasta_partition_round_robin(size_t cur_parallelism);  

    bool process_backward_edges(); // process backward edges based on std::list
    bool process_backward_edges_taskflow(bool use_round_robin = false); // process backward edges based on node class

    void generate_topo_order();

    bool check_topo_iterators_and_pos() const;
    bool is_topo_nodes_valid() const;
    bool validate_modify_edge() const;
    void print_topo_order() const;
    void add_backward_edge();
    void remove_actual_edge();

    // helper: verify run_graph_cudaflow_partition_update()
    bool verify_cudaflow_partition_update(size_t num_streams);

    // helper: verify run_graph_cudaflow_partition_update_incre()
    bool verify_cudaflow_partition_update_incre(size_t num_streams);

    // helper: verify Taskflow consistency with my graph object
    bool is_taskflow_topo_consistent();

    // helper: verify if Taskflow acts like a linear chain
    // Returns true iff the taskflow has max parallelism 1,
    // i.e. there is never more than one ready task in a topological execution.
    bool is_taskflow_linear_chain();

    // helper: verify if _linked_to matches _topo_nodes
    // Also verify if _linked_to_is_actual is correct
    bool is_linked_to_match_topo();

    // helper: verify if _breakable_nodes includes all the breakable nodes
    bool is_breakable_nodes_complete();

    // helper: verify if cudaflow partitioned taskflow satisfies limited parallelism
    bool is_cudaflow_satisfy_parallelism(size_t cur_parallelism);

  private:

    bool _initialized = false;

    size_t _partition_size = 0;
    int _max_cluster_id = -1; // record the largest cluster id

    std::list<Node> _nodes;
    std::list<Edge> _edges;
    std::list<CNode> _cnodes;
    std::list<CEdge> _cedges;

    std::list<Node*> _topo_nodes;
    std::queue<Edge*> _backward_edges;
    // _breakable_nodes stores the breakable points in the linear chain.
    // _breakable_nodes[i] is the last node of one segments.
    // _breakable_nodes.size() = maximum potential parallelism - 1
    std::vector<Node*> _breakable_nodes;

    uint64_t _next_node_uid = 0;  // monotonic counter for generating unique node names

    /*
      --------- For cudaflow partitioning -------------
    */
    std::vector<std::list<Node*>> _level_list;

    // Scratch buffer for _assign_nodes_to_streams. Hoisted to a member so capacity
    // is retained across iterations, avoiding repeated heap allocations.
    std::vector<std::vector<Node*>> _node_streams_scratch;

    // filling up _level_list for cudaflow 
    void _get_level_list_for_cudaflow();

    // assign nodes to streams level by level
    void _assign_nodes_to_streams(size_t num_streams);

    // ensure _level_list has at least (k+1) levels
    void _ensure_level_exists(size_t k);

    // place node n at level k (assumes _level stale, _level_it may be stale; overwrites both)
    void _place_node_at_level(Node* n, int k);

    // recompute L(n) from current fanins
    int _recompute_level(Node* n) const;

    // incremental updaters to call from insert_edge/remove_edge
    void _incre_level_on_insert_edge(Node* u, Node* v);
    void _incre_level_on_remove_edge(Node* u, Node* v);

    /*
      -------------------------------------------------
    */

    bool _is_taskflow_acyclic(); 

    bool _check_cudaflow_partition_update(size_t num_streams); 

    // get level list of current graph 
    std::vector<std::vector<Node*>> _get_level_list();
    std::vector<std::vector<Node*>> _get_taskflow_level_list();

    // get topological order of current graph using BFS
    std::vector<Node*> _get_topo_order_bfs();

    // get topological order of current graph using BFS
    std::vector<Node*> _get_topo_order_dfs(); 

    // get reversed topological order of current graph using DFS 
    void _get_topo_reverse_order_dfs(std::vector<Node*>& topo); 

    bool _has_original_edge(Node* from, Node* to) const; 

    template <typename T>
    void _topo_dfs(std::vector<T*>& topo_order, T* node);

    void _assign_cluster_id(Node* node_ptr, std::vector<std::atomic<size_t>>& cluster_cnt, std::atomic<int>& max_cluster_id);

    void _build_partitioned_graph();

    uint64_t _cur_dfs_tag = 1; // this tag should be enough for 1k incremental iterations
                               // each iteration we increment the tag multiple times
    bool _restricted_dfs(Node* from, Node* to);

    // left  | k nodes_to_relabel | right
    void _relabel_after_from_until(Node* left, Node* right, size_t k);

    // left  | k nodes_to_relabel | (empty, end) 
    void _relabel_after_left_to_end(Node* left, size_t k);

    // perform a full relabel
    void _relabel_full();

    // construct taskflow graph as a linear chain
    void _construct_taskflow_linear_chain(size_t matrix_size); 

    // build breakable points
    void _build_breakable_nodes();

    // select breakable points
    std::vector<Node*> _select_breakable_nodes(size_t cur_parallelism) const;

    // help function to update taskflow sequence
    void _update_taskflow_sequence(Node* left_update_bound, Node* right_update_bound);

    // helper to check if we need to rebuild breakable points
    bool _need_to_rebuild_breakable_nodes() const;

    // helper to get max parallelism of taskflow
    size_t _get_max_parallelism_taskflow();

    // helper to get critical path of taskflow
    size_t _get_critical_path_length_taskflow();

    // incremental update with semaphore runtime
    size_t _incre_runtime_with_semaphore = 0;
    size_t _incre_runtime_with_semaphore_graph_construct = 0;

    // incremental update with cudaflow_partition runtime
    size_t _incre_runtime_with_cudaflow_partition = 0;
    size_t _incre_partition_runtime_with_cudaflow_partition = 0;
    size_t _incre_construct_runtime_with_cudaflow = 0;

    // semaphore 
    size_t _semaphore_taskflow_buildtime = 0;
    size_t _semaphore_taskflow_runtime = 0;

    // cudaflow partitioning.
    // Apply full partitioning (topological sort) for each incremental iteration 
    size_t _cudaflow_partitioning_runtime = 0;
    size_t _cudaflow_rebuild_level_list_runtime = 0;
    size_t _cudaflow_incre_level_list_runtime = 0;
    size_t _cudaflow_assign_streams_runtime = 0;
    size_t _cudaflow_taskflow_buildtime = 0;
    size_t _cudaflow_taskflow_runtime = 0;

    // pasta partitoning.
    // Maintain topological order (by processing backward edges) for each incremental iteration 
    size_t _pasta_partitioning_runtime = 0;
    size_t _pasta_taskflow_buildtime = 0;
    size_t _pasta_taskflow_runtime = 0;
    size_t _critical_path_length_original = 0;
    size_t _critical_path_length_constrained = 0;
    size_t _num_breakable_nodes;
    size_t _num_selected_breakable_nodes;

    tf::Taskflow _taskflow;
    tf::Executor _executor{std::thread::hardware_concurrency()};
    tf::Semaphore _semaphore{std::thread::hardware_concurrency()};  
    bool _first_run = true;

    size_t _process_backward_edge_time = 0;
    size_t _generate_topo_order_time = 0;
    size_t _num_backward_edges = 0;

};

} // end of namespace pasta


























