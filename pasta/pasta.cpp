#include "pasta.hpp"

namespace pasta {

Graph::Graph(const std::string& filename) {

  /*
    file format example:
    3
    "A";
    "B";
    "C";
    "A" -> "B";
    "B" -> "C";
  */

  std::ifstream infile(filename);
  if(!infile) {
    std::cerr << "Error opening file.\n";
    std::exit(EXIT_FAILURE);
  }

  size_t num_nodes;
  // read the number of nodes
  infile >> num_nodes;

  std::unordered_map<std::string, Node*> name_map;

  // read node names and add them to the graph
  std::string node_name;
  for(size_t i=0; i<num_nodes; i++) {
    infile >> node_name;
    // remove quotes from node name
    node_name = node_name.substr(1, node_name.size()-3);
    name_map[node_name] = insert_node(node_name);
  }

  // read edges and add them to the graph
  std::string from, to, arrow;
  while(infile >> from >> arrow >> to) {
    from = from.substr(1, from.size()-2);
    to = to.substr(1, to.size()-3);
    insert_edge(name_map[from], name_map[to]);
  }
}

Node* Graph::insert_node(const std::string& name) {

  Node* new_node = &(_nodes.emplace_back(name));
  return new_node;
}

Edge* Graph::insert_edge(Node* from, Node* to) {

  // construct edge object within vector
  Edge* new_edge = &(_edges.emplace_back());

  // set from and to node for this edge
  new_edge->_from = from;
  new_edge->_to = to;

  // add fanout(fanin) for from(to) node
  from->_fanouts.push_back(new_edge);
  to->_fanins.push_back(new_edge);

  return new_edge;
}

bool Graph::has_cycle_before_partition() {

  // reset
  for(auto& node : _nodes) {
    node._visited = false;
  }

  std::vector<Node*> topo_order;
  for(auto& node : _nodes) {
    if(node._fanins.size() == 0) {
      _topo_dfs(topo_order, &node);
    }
  }

  // if the size of topological sequence is equal to
  // the total number of nodes, then no cycle
  if(topo_order.size() == _nodes.size()) {
    return false;
  }
  else {
    return true;
  }
}

bool Graph::has_cycle_after_partition() {

  // reset
  for(auto& cnode : _cnodes) {
    cnode._visited = false;
  }

  std::vector<CNode*> topo_order;
  for(auto& cnode : _cnodes) {
    if(cnode._fanins.size() == 0) {
      _topo_dfs(topo_order, &cnode);
    }
  }

  // if the size of topological sequence is equal to
  // the total number of cnodes, then no cycle
  if(topo_order.size() == _cnodes.size()) {
    return false;
  }
  else {
    return true;
  }
}

template <typename T>
void Graph::_topo_dfs(std::vector<T*>& topo_order, T* node) {

  node->_visited = true;
  for(auto fanout : node->_fanouts) {
    T* successor = fanout->_to;
    if(!successor->_visited) {
      _topo_dfs(topo_order, successor);
    }
  }
  topo_order.push_back(node);
}

void Graph::partition_c_pasta() {

  // check partition_size before partition
  if(_partition_size == 0) {
    std::cerr << "please set partition size before partition.\n";
    std::exit(EXIT_FAILURE);
  }

  // reset
  _max_cluster_id = -1;
  for(auto& node : _nodes) {
    node._dep_cnt = 0;
    node._cluster_id = -1;
  }

  // initialize threadpool and work stealing queues
  size_t num_threads = std::thread::hardware_concurrency();
  std::vector<std::thread> threads;
  std::vector<WorkStealingQueue<Node*>> queues(num_threads);
  std::atomic<size_t> node_cnt = 0; // count the num of nodes partitioned

  // put all source nodes into the first wsq
  int cur_cluster_id = -1;
  for(auto& node : _nodes) {
    if(node._fanins.size() == 0) {
      ++cur_cluster_id;
      node._cluster_id = cur_cluster_id;
      queues[0].push(&node);
    }
  }

  // initialize counters for cluster size
  std::atomic<int> max_cluster_id = cur_cluster_id;
  std::vector<std::atomic<size_t>> cluster_cnt(_nodes.size()); // we will have at most _nodes.size() clusters
  for(size_t i=0; i<_nodes.size(); i++) {
    cluster_cnt[i] = 0;
  }

  /*
   * emplace tasks into threadpool
   * task starts to execute the moment it is in the threadpool
   */
  for(size_t i=0; i<num_threads; i++) {
    threads.emplace_back([this, i, &cluster_cnt, &max_cluster_id, &node_cnt, &queues, num_threads]() {
      while(node_cnt.load(std::memory_order_relaxed) < _nodes.size()) {

        std::optional<Node*> node_ptr_opt;

        // first process tasks in thread i's own queue
        while(!queues[i].empty()) {
          node_ptr_opt = queues[i].pop();
          if(node_ptr_opt.has_value()) { // if get the node successfully
            Node* node_ptr = node_ptr_opt.value();
            node_cnt.fetch_add(1, std::memory_order_relaxed);
            _assign_cluster_id(node_ptr, cluster_cnt, max_cluster_id);
            /*
             * process linear chain
             * if this node leads a linear chain
             * there is no need to push its successors into queue
             */
            while(node_ptr->_fanouts.size() == 1) {
              Node* successor = (*(node_ptr->_fanouts.begin()))->_to;
              if(successor->_fanins.size() != 1) {
                // check if it is a linear chain
                break;
              }
              node_ptr = successor;
              node_ptr->_dep_cnt.fetch_add(1);
              node_cnt.fetch_add(1, std::memory_order_relaxed);
              _assign_cluster_id(node_ptr, cluster_cnt, max_cluster_id);
            }
            // process successors: release the dependents
            for(auto edge : node_ptr->_fanouts) {
              Node* successor = edge->_to;
              if(successor->_dep_cnt.fetch_add(1, std::memory_order_relaxed) == successor->_fanins.size() - 1) {
                queues[i].push(successor);
              }
            }
          }
        }

        // steal tasks from other threads' queues if its own queue is empty
        for(size_t j=0; j<num_threads; j++) {
          if(j == i) {
            continue;
          }
          node_ptr_opt = queues[j].steal();
          if(node_ptr_opt.has_value()) {
            break; // successfully steal one task
          }
        }
        if(!node_ptr_opt.has_value()) {
          continue; // nothing to steal after traversal
        }
        // process the stolen task
        Node* node_ptr = node_ptr_opt.value();
        node_cnt.fetch_add(1, std::memory_order_relaxed);
        _assign_cluster_id(node_ptr, cluster_cnt, max_cluster_id);
        // process linear chain
        while(node_ptr->_fanouts.size() == 1) {
          Node* successor = (*(node_ptr->_fanouts.begin()))->_to;
          if(successor->_fanins.size() != 1) { // check linear chain
            break;
          }
          node_ptr = successor;
          node_ptr->_dep_cnt.fetch_add(1);
          node_cnt.fetch_add(1, std::memory_order_relaxed);
          _assign_cluster_id(node_ptr, cluster_cnt, max_cluster_id);
        }
        // process successors: release the dependents
        for(auto edge : node_ptr->_fanouts) {
          Node* successor = edge->_to;
          if(successor->_dep_cnt.fetch_add(1, std::memory_order_relaxed) == successor->_fanins.size() - 1) {
            queues[i].push(successor);
          }
        }
      }
    });
  }

  // join the threads
  for(auto& thread : threads) {
    thread.join();
  }

  // record largest cluster id
  _max_cluster_id = max_cluster_id.load();

  // build partitioned graph
  _build_partitioned_graph();
}

void Graph::_assign_cluster_id(Node* node_ptr, std::vector<std::atomic<size_t>>& cluster_cnt, std::atomic<int>& max_cluster_id) {

  int desired_cluster_id = node_ptr->_cluster_id; // cluster_id is initialized as -1(excluding source tasks)

  // choose the largest cluster_id from its dependents as its desired_cluster_id
  for(auto edge_ptr : node_ptr->_fanins) {
    Node* dep_ptr = edge_ptr->_from; // dependent of node_ptr
    if(dep_ptr->_cluster_id > desired_cluster_id) {
      desired_cluster_id = dep_ptr->_cluster_id;
    }
  }

  // check if the desired cluster still has space for this node
  if(cluster_cnt[desired_cluster_id].fetch_add(1, std::memory_order_relaxed) < _partition_size) {
    node_ptr->_cluster_id = desired_cluster_id;
  }
  // if no, create a new cluster_id by ++max_cluster_id
  else {
    int new_cluster_id = max_cluster_id.fetch_add(1, std::memory_order_relaxed) + 1;
    node_ptr->_cluster_id = new_cluster_id;
    cluster_cnt[new_cluster_id]++;
  }
}

void Graph::_build_partitioned_graph() {

  // clear the original graph
  _cnodes.clear();
  _cedges.clear();

  if(_max_cluster_id < 0) {
    std::cerr << "partition failed: _max_cluster_id is wrong...\n";
    std::exit(EXIT_FAILURE);
  }
  size_t num_clusters = _max_cluster_id + 1;

  // use a 2-D vector to record clusters (cuz it supports constant time random access)
  std::vector<std::vector<Node*>> clusters(num_clusters);
  for(auto& node : _nodes) {
    int cluster = node._cluster_id;
    clusters[cluster].push_back(&node);
  }

  // construct CNode
  for(size_t i=0; i<num_clusters; i++) {
    CNode* cnode_ptr = &(_cnodes.emplace_back());
    if(clusters[i].size() == 0) {
      continue;
    }
    for(auto node_ptr : clusters[i]) {
      cnode_ptr->_nodes.emplace_back(node_ptr);
      node_ptr->_cnode = cnode_ptr;
    }
  }

  // construct CEdge
  // e.g., to find fanouts,
  // 1. traverse nodes within a cluster node
  // 2. for each node, find their fanout nodes
  // 3. find the cluster nodes to which these fanout nodes belong
  // 4. add edge
  // Note. Redundent edges will be added.
  size_t itr = 0; // to iterate clusters
  for(auto& cnode : _cnodes) {
    // add fanouts
    for(auto node_ptr : clusters[itr]) {
      for(auto node_fanouts : node_ptr->_fanouts) {
        Node* successor_ptr = node_fanouts->_to;
        // if this node is already in the cluster, ignore it
        if(successor_ptr->_cluster_id == node_ptr->_cluster_id) {
          continue;
        }
        CNode* to = successor_ptr->_cnode;
        CEdge* cedge_ptr = &(_cedges.emplace_back());
        cedge_ptr->_from = &cnode;
        cedge_ptr->_to = to;
        cnode._fanouts.emplace_back(cedge_ptr);
        to->_fanins.emplace_back(cedge_ptr);
      }
    }

    // add fanins
    for(auto node_ptr : clusters[itr]) {
      for(auto node_fanins : node_ptr->_fanins) {
        Node* dependent_ptr = node_fanins->_from;
        // if this node is already in the cluster, ignore it
        if(dependent_ptr->_cluster_id == node_ptr->_cluster_id) {
          continue;
        }
        CNode* from = dependent_ptr->_cnode;
        CEdge* cedge_ptr = &(_cedges.emplace_back());
        cedge_ptr->_to = &cnode;
        cedge_ptr->_from = from;
        cnode._fanins.emplace_back(cedge_ptr);
        from->_fanouts.emplace_back(cedge_ptr);
      }
    }
      // remove the duplicates
    cnode._fanouts.unique();
    cnode._fanins.unique();

    ++itr;
  }
}

void Graph::run_graph_before_partition(size_t matrix_size) {

  tf::Taskflow taskflow;
  tf::Executor executor;

  for(auto& node : _nodes) {
    node._task = taskflow.emplace([this, matrix_size]() {
      // std::this_thread::sleep_for(std::chrono::nanoseconds(task_runtime));
      size_t N = matrix_size;
      size_t M = matrix_size;
      size_t K = matrix_size;
      std::vector<int> A(N*K, 1);
      std::vector<int> B(K*M, 2);
      std::vector<int> C(N*M);
      for(size_t n=0; n<N; n++) {
        for(size_t m=0; m<M; m++) {
          int temp = 0;
          for(size_t k=0; k<K; k++) {
            temp += A[n*K + k] * B[k*M + m];
          }
          C[n*M + m] = temp;
        }
      }
    });
  }

  for(auto& node : _nodes) {
    for(auto fanout : node._fanouts) {
      node._task.precede(fanout->_to->_task);
    }
  }

  auto start = std::chrono::steady_clock::now();
  executor.run(taskflow).wait();
  auto end = std::chrono::steady_clock::now();
  size_t origin_taskflow_runtime = std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();

  std::cout << "origin_taskflow_runtime: " << origin_taskflow_runtime
            << " us\n";
}

void Graph::run_graph_after_partition(size_t matrix_size) {

  if(_max_cluster_id < 0) {
    std::cerr << "partition failed: _max_cluster_id is wrong...\n";
    std::exit(EXIT_FAILURE);
  }

  tf::Taskflow taskflow;
  tf::Executor executor;

  for(auto& cnode : _cnodes) {
    cnode._task = taskflow.emplace([&cnode, matrix_size]() {
      for(size_t i=0; i<cnode._nodes.size(); i++) {
        // std::this_thread::sleep_for(std::chrono::nanoseconds(task_runtime));
        size_t N = matrix_size;
        size_t M = matrix_size;
        size_t K = matrix_size;
        std::vector<int> A(N*K, 1);
        std::vector<int> B(K*M, 2);
        std::vector<int> C(N*M);
        for(size_t n=0; n<N; n++) {
          for(size_t m=0; m<M; m++) {
            int temp = 0;
            for(size_t k=0; k<K; k++) {
              temp += A[n*K + k] * B[k*M + m];
            }
            C[n*M + m] = temp;
          }
        }
      }
    });
  }

  for(auto& cnode : _cnodes) {
    for(auto fanout : cnode._fanouts) {
      cnode._task.precede(fanout->_to->_task);
    }
  }

  auto start = std::chrono::steady_clock::now();
  executor.run(taskflow).wait();
  auto end = std::chrono::steady_clock::now();
  size_t partitioned_taskflow_runtime = std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();

  std::cout << "partitioned_taskflow_runtime: " << partitioned_taskflow_runtime
            << " us\n";
}

void Graph::dump_graph() {

  tf::Taskflow taskflow;
  tf::Executor executor;

  auto start = std::chrono::steady_clock::now();
  for(auto& node : _nodes) {
    node._task = taskflow.emplace([this]() {
    }).name(std::to_string(node._cluster_id));
  }

  for(auto& node : _nodes) {
    for(auto fanout : node._fanouts) {
      node._task.precede(fanout->_to->_task);
    }
  }

  taskflow.dump(std::cout);
}

} // end of namespace pasta
