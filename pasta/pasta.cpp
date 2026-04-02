#include "pasta.hpp"

namespace pasta {

Graph::Graph(const std::string& filename, RunMode mode, size_t matrix_size) {

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
    if(mode == RunMode::IncrementalPartition) {
      name_map[node_name] = insert_node(node_name, mode, matrix_size);
    }
    else {
      name_map[node_name] = insert_node(node_name);
    }
  }

  // read edges and add them to the graph
  std::string from, to, arrow;
  while(infile >> from >> arrow >> to) {
    from = from.substr(1, from.size()-2);
    to = to.substr(1, to.size()-3);
    if(mode == RunMode::IncrementalPartition) {
      insert_edge(name_map[from], name_map[to], mode);
    }
    else {
      insert_edge(name_map[from], name_map[to]);
    }
  }

  // initialize topological seqenuce after constructing the graph
  std::vector<Node*> topo_dfs = _get_topo_order_dfs(); 

  // assign linked fanin/fanout based on topological sequence
  // assign pos value
  uint64_t pos = 0;
  for(int i=0; i<_nodes.size(); i++) {
    _topo_nodes.push_back(topo_dfs[i]);
    topo_dfs[i]->_topo_it = std::prev(_topo_nodes.end());
    topo_dfs[i]->_pos = pos;
    pos += 1024;
  }

  // check _topo_it
  for(auto it = _topo_nodes.begin(); it != _topo_nodes.end(); it++) {
    Node* node = *it;
    assert(node->_topo_it == it);
  }

  // identify actual edge vs extra edge
  for(auto it = _topo_nodes.begin(); it != std::prev(_topo_nodes.end()); ++it) {
    Node* u = *it;
    Node* v = *std::next(it);
    u->_linked_to = v;
    u->_linked_to_is_actual = _has_original_edge(u, v);
  }

  _initialized = true;

}

Node* Graph::insert_node(const std::string& name, RunMode mode, size_t matrix_size) {

  // Node node(name);
  int id = (int)_nodes.size();
  Node* node_ptr = &(_nodes.emplace_back(name));
  node_ptr->_node_satellite = --_nodes.end();
  node_ptr->_id = id;

  auto start_construct = std::chrono::steady_clock::now();
  // if run taskflow with semaphore
  if(mode == RunMode::Semaphore || mode == RunMode::IncrementalPartition) {
    node_ptr->_task = _taskflow.emplace([this, matrix_size]() {
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
    }).name(node_ptr->_name);
    node_ptr->_task.acquire(_semaphore);
    node_ptr->_task.release(_semaphore);
  }
  auto end_construct = std::chrono::steady_clock::now();
  size_t taskflow_constucttime = std::chrono::duration_cast<std::chrono::microseconds>(end_construct-start_construct).count();
  // we won't run semaphore and partitioning in the same program so I just add them both
  _incre_runtime_with_semaphore_graph_construct += taskflow_constucttime;
  _incre_construct_runtime_with_cudaflow += taskflow_constucttime;

  return node_ptr;
}

Edge* Graph::insert_edge(Node* from, Node* to, RunMode mode) {

  // std::cout << "insert " << from->_name << " -> " << to->_name << "\n";

  // update _fanout_set
  from->_fanout_set.insert(to);

  // We only mark _linked_to_is_actual here for forward edges.
  // Backward edges will be handled in process_backward_edges_taskflow().
  if(_initialized && from->_pos < to->_pos) {
    // if from->_pos < to->_pos, then from is not the last node
    auto it = from->_topo_it;
    Node* next = *(std::next(it));
    if(next == to) {
      from->_linked_to_is_actual = true;
    }
  }

  // Edge edge;
  Edge* edge_ptr = &_edges.emplace_back();

  edge_ptr->_from = from;
  edge_ptr->_to = to;

  from->_fanouts.push_back(edge_ptr);
  to->_fanins.push_back(edge_ptr);

  // tells the index of this edge in _fanouts of from nodes and _fanins of to nodes
  // for remove_edge()
  edge_ptr->_from_satellite = --from->_fanouts.end();
  edge_ptr->_to_satellite = --to->_fanins.end();

  // tells the index of this edge in _fanouts of from nodes and _fanins of to nodes
  // but make the index as a pair with nodes, for remove_node()
  from->_fanout_satellites.push_back(std::make_pair(to, --to->_fanins.end()));
  to->_fanin_satellites.push_back(std::make_pair(from, --from->_fanouts.end()));

  edge_ptr->_satellite = --_edges.end();

  auto start_construct = std::chrono::steady_clock::now();
  // if run taskflow with semaphore
  if(mode == RunMode::Semaphore || mode == RunMode::IncrementalPartition) {
    // std::cerr << "taskflow insert: " << from->_name << " -> " << to->_name << "\n";
    from->_task.precede(to->_task);
  }
  auto end_construct = std::chrono::steady_clock::now();
  size_t taskflow_constucttime = std::chrono::duration_cast<std::chrono::microseconds>(end_construct-start_construct).count();
  _incre_runtime_with_semaphore_graph_construct += taskflow_constucttime;

  // check if this is a backward edge by comparing _pos
  if(from->_pos > to->_pos) {
    _backward_edges.push(edge_ptr);
  }

  return edge_ptr;
}

void Graph::remove_node(Node* node, RunMode mode) {

  // remove its fanin/fanout edges from _edges
  // remove_edge will erase this edge from node->_fanins/fanouts, so no need to pop_front()
  while(!node->_fanins.empty()) {
    Edge* from = node->_fanins.front();
    remove_edge(from);
  }
  while(!node->_fanouts.empty()) {
    Edge* to = node->_fanouts.front();
    remove_edge(to);
  }
  
  auto start_construct = std::chrono::steady_clock::now();
  // if run taskflow with semaphore
  if(mode == RunMode::Semaphore) {
    _taskflow.erase(node->_task);
  }
  auto end_construct = std::chrono::steady_clock::now();
  size_t taskflow_constucttime = std::chrono::duration_cast<std::chrono::microseconds>(end_construct-start_construct).count();
  _incre_runtime_with_semaphore_graph_construct += taskflow_constucttime;
  _incre_construct_runtime_with_cudaflow += taskflow_constucttime;

  _nodes.erase(node->_node_satellite);
}

void Graph::remove_edge(Edge* edge, RunMode mode) {

  Node* from = edge->_from;
  Node* to = edge->_to;

  // std::cout << "remove " << from->_name << " -> " << to->_name << "\n";

  // update _fanout_set
  from->_fanout_set.erase(to);

  // Since I always remove edges first, so from is not the last one
  // But it is not safe to assume that
  if(from->_pos < to->_pos) { 
    // use this condition to ensure from is not the last one
    auto it = from->_topo_it;
    Node* next = *(std::next(it));
    if(next == to) {
      from->_linked_to_is_actual = false;
      // This introduces new breakable node
      _breakable_nodes.push_back(from);
    }
  }

  // remove edge from _fanouts of from node
  // also remove edge from _fanout_satellites of from node
  // this edge should be in the same index as the edge in _fanouts
  // because they are always inserted and removed at the same time
  auto index = std::distance(from->_fanouts.begin(), edge->_from_satellite);
  auto it_satellite = from->_fanout_satellites.begin();
  std::advance(it_satellite, index);
  from->_fanouts.erase(edge->_from_satellite);
  from->_fanout_satellites.erase(it_satellite);

  // same method applied to to node
  index = std::distance(to->_fanins.begin(), edge->_to_satellite);
  it_satellite = to->_fanin_satellites.begin();
  std::advance(it_satellite, index);
  to->_fanins.erase(edge->_to_satellite);
  to->_fanin_satellites.erase(it_satellite);

  auto start_construct = std::chrono::steady_clock::now();
  // if run taskflow with semaphore
  if(mode == RunMode::Semaphore || mode == RunMode::IncrementalPartition) {
    from->_task.remove_successors(to->_task);
    to->_task.remove_predecessors(from->_task);
  }
  auto end_construct = std::chrono::steady_clock::now();
  size_t taskflow_constucttime = std::chrono::duration_cast<std::chrono::microseconds>(end_construct-start_construct).count();
  _incre_runtime_with_semaphore_graph_construct += taskflow_constucttime;
  _incre_construct_runtime_with_cudaflow += taskflow_constucttime;

  _edges.erase(edge->_satellite);
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

void Graph::remove_random_nodes(size_t N, std::mt19937& gen, RunMode mode) {

  N = std::min(N, _nodes.size());
  if (N == 0) return;

  // collect pointers
  std::vector<Node*> cand;
  cand.reserve(_nodes.size());
  for (auto& n : _nodes) cand.push_back(std::addressof(n));

  std::shuffle(cand.begin(), cand.end(), gen);
  cand.resize(N);

  for (Node* p : cand) remove_node(p, mode); // your existing internal removal

}

void Graph::remove_random_edges(size_t N, std::mt19937& gen, RunMode mode) {

  N = std::min(N, _edges.size());
  if (N == 0) return;

  std::vector<Edge*> cand;
  cand.reserve(_edges.size());
  for (auto& e : _edges) cand.push_back(std::addressof(e));

  std::shuffle(cand.begin(), cand.end(), gen);
  cand.resize(N);

  for (Edge* p : cand) {
    remove_edge(p, mode);
  }
}

size_t Graph::add_random_edges(size_t N, std::mt19937& gen, size_t max_tries_multiplier, RunMode mode) {

  std::vector<Node*> topo;
  topo.reserve(_nodes.size());
  _get_topo_reverse_order_dfs(topo);

  if (topo.size() < 2 || N == 0) return 0;

  // _get_topo_reverse_order_dfs() is reverse topo because _topo_dfs pushes after recursion
  std::reverse(topo.begin(), topo.end());

  const size_t n = topo.size();

  // Max possible edges under this ordering is n*(n-1)/2; clamp N to avoid nonsense.
  const size_t max_possible = n * (n - 1) / 2;
  if (N > max_possible) N = max_possible;

  auto has_edge = [](Node* from, Node* to) -> bool {
    for (auto* e : from->_fanouts) {
      if (e->_to == to) return true;
    }
    return false;
  };

  size_t added = 0;
  const size_t max_tries = max_tries_multiplier * N + 100;

  std::uniform_int_distribution<size_t> dis_i(0, n - 2);

  for (size_t tries = 0; tries < max_tries && added < N; ++tries) {
    const size_t i = dis_i(gen);
    std::uniform_int_distribution<size_t> dis_j(i + 1, n - 1);
    const size_t j = dis_j(gen);

    Node* from = topo[i];
    Node* to   = topo[j];

    // avoid duplicates
    if (has_edge(from, to)) continue;

    insert_edge(from, to, mode);
    ++added;
  }

  return added;  // could be < N if graph is already dense
}

std::vector<Node*> Graph::add_random_nodes(size_t N, std::mt19937& gen, 
                                           const std::string& name_prefix, 
                                           RunMode mode, size_t matrix_size) {
  std::vector<Node*> old_nodes;
  old_nodes.reserve(_nodes.size());
  for (auto& n : _nodes) {
    old_nodes.push_back(std::addressof(n));
  }

  std::vector<Node*> new_nodes;
  new_nodes.reserve(N);

  // 1) insert nodes
  for (size_t i = 0; i < N; ++i) {
    // Make names unique-ish; you can replace with your own global "iteration count"
    std::string name = name_prefix + "_" + std::to_string(_nodes.size()) + "_" + std::to_string(i);
    new_nodes.push_back(insert_node(name, mode, matrix_size));
  }

  // If there were no old nodes, we can't connect to existing nodes
  if (old_nodes.empty()) return new_nodes;

  auto has_edge = [](Node* from, Node* to) -> bool {
    for (auto* e : from->_fanouts) {
      if (e->_to == to) return true;
    }
    return false;
  };

  // 2) connect each new node with one random existing node
  std::uniform_int_distribution<size_t> pick_old(0, old_nodes.size() - 1);
  std::bernoulli_distribution coin(0.5);

  for (Node* nn : new_nodes) {
    Node* ex = old_nodes[pick_old(gen)];

    // Random direction, but always safe because nn is brand new (no other edges yet)
    if (coin(gen)) {
      if (!has_edge(ex, nn)) {
        insert_edge(ex, nn, mode);      // existing -> new
      }
    } else {
      if (!has_edge(nn, ex)) {
        insert_edge(nn, ex, mode);      // new -> existing
      }
    }
  }

  return new_nodes;
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
            << " ms\n";
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

void Graph::_get_topo_reverse_order_dfs(std::vector<Node*>& topo) { 

  // reset
  for(auto& node : _nodes) {
    node._visited = false;
  }

  for(auto& node : _nodes) {
    if(node._fanins.size() == 0) {
      _topo_dfs(topo, &node);
    }
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

void Graph::run_graph_semaphore(size_t matrix_size, size_t num_semaphore) {

  // std::cout << "total #threads available: " << std::thread::hardware_concurrency() << "\n";

  // _taskflow.clear();
  _semaphore.reset(num_semaphore);

  auto start_construct = std::chrono::steady_clock::now();
  if(_first_run) {
    for(auto& node : _nodes) {
      node._task = _taskflow.emplace([this, matrix_size, &node]() {
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

    for(auto& node : _nodes) {
      node._task.acquire(_semaphore);
      node._task.release(_semaphore);
    }
  }
  auto end_construct = std::chrono::steady_clock::now();
  size_t taskflow_constucttime = std::chrono::duration_cast<std::chrono::microseconds>(end_construct-start_construct).count();
  _incre_runtime_with_semaphore_graph_construct += taskflow_constucttime;

  _first_run = false;

  auto start = std::chrono::steady_clock::now();
  _executor.run(_taskflow).wait();
  auto end = std::chrono::steady_clock::now();
  size_t taskflow_runtime = std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();
  _incre_runtime_with_semaphore += taskflow_runtime;

  // printf("For current iteration, taskflow runtime with #semaphores = %ld: %ld ms\n", num_semaphore, taskflow_runtime);
}

void Graph::dump_graph() {
  // Create a local Taskflow object for dumping
  tf::Taskflow local_taskflow;

  // Create a local mapping of tasks
  std::unordered_map<Node*, tf::Task> task_map;

  for (auto& node : _nodes) {
    // Ensure node is a pointer
    pasta::Node* node_ptr = &node;

    // Create a task for each node
    task_map[node_ptr] = local_taskflow.emplace([node_ptr]() {
    // Task body can be customized if needed
    }).name(node_ptr->_name);
  }

  // Define the dependencies
  for (auto& node : _nodes) {
    for (auto fanout : node._fanouts) {
      pasta::Node* node_ptr = &node;
      task_map[node_ptr].precede(task_map[fanout->_to]);
    }
  }

  // Dump the graph to the output stream
  local_taskflow.dump(std::cout);
}

std::vector<Node*> Graph::_get_topo_order_bfs() {

  std::vector<Node*> topo;

  std::vector<int> indegrees(_nodes.size(), 0);
  for(auto& node : _nodes) {
    indegrees[node._id] = (int)node._fanins.size();
  }

  std::queue<Node*> q;
  for(auto& node : _nodes) {
    if(node._fanins.size() == 0) {
      node._level = 0;
      q.push(&node);
    }
  }

  while(!q.empty()) {
    
    Node* cur = q.front();
    q.pop();

    topo.push_back(cur);

    for(auto fanout : cur->_fanouts) {
      Node* fanout_node = fanout->_to;
      if(--indegrees[fanout_node->_id] == 0) {
        fanout_node->_level = ++cur->_level;
        q.push(fanout_node);
      }
    }
  }

  return topo; 
}

void Graph::test_func() {

  // dump_graph();

  // for(auto node_ptr : _topo_nodes) {
  //   std::cout << node_ptr->_name << "(" << node_ptr->_pos << ") ";
  // }
  // std::cout << "\n";

  if(!process_backward_edges()) {
    throw std::runtime_error("The topological order is not maintained correctly");
  }

}

std::vector<std::vector<Node*>> Graph::_get_taskflow_level_list() {
  std::vector<tf::Task> tasks;
  tasks.reserve(_taskflow.num_tasks());

  // task name -> index in tasks
  std::unordered_map<std::string, size_t> task_index;
  task_index.reserve(_taskflow.num_tasks());

  // collect all tasks
  _taskflow.for_each_task([&](tf::Task t) {
    task_index[t.name()] = tasks.size();
    tasks.push_back(t);
  });

  std::vector<std::vector<Node*>> levels;
  if(tasks.empty()) {
    return levels;
  }

  // node name -> Node*
  std::unordered_map<std::string, Node*> name_to_node;
  name_to_node.reserve(_nodes.size());

  for(auto& node : _nodes) {
    name_to_node[node._name] = &node;
  }

  // indegree
  std::vector<size_t> indeg(tasks.size());
  for(size_t i = 0; i < tasks.size(); ++i) {
    indeg[i] = tasks[i].num_predecessors();
  }

  // initial ready set
  std::vector<size_t> curr;
  for(size_t i = 0; i < tasks.size(); ++i) {
    if(indeg[i] == 0) {
      curr.push_back(i);
    }
  }

  size_t visited = 0;

  while(!curr.empty()) {
    std::vector<Node*> level;
    std::vector<size_t> next;

    level.reserve(curr.size());

    for(auto u : curr) {
      ++visited;

      const auto& task = tasks[u];

      auto nit = name_to_node.find(task.name());
      if(nit == name_to_node.end()) {
        throw std::runtime_error("cannot find Node* for task " + task.name());
      }

      level.push_back(nit->second);

      task.for_each_successor([&](tf::Task succ) {
        auto sit = task_index.find(succ.name());
        if(sit == task_index.end()) {
          throw std::runtime_error("cannot find successor index for task " + succ.name());
        }

        size_t v = sit->second;
        if(--indeg[v] == 0) {
          next.push_back(v);
        }
      });
    }

    levels.push_back(std::move(level));
    curr = std::move(next);
  }

  if(visited != tasks.size()) {
    throw std::runtime_error("Taskflow graph is not a DAG");
  }

  return levels;
}

std::vector<std::vector<Node*>> Graph::_get_level_list() {

  std::vector<std::vector<Node*>> level_list;

  std::vector<int> indegrees(_nodes.size(), 0);
  for(auto& node : _nodes) {
    indegrees[node._id] = node._fanins.size();
  }

  std::queue<Node*> q;
  for(auto& node : _nodes) {
    if(node._fanins.size() == 0) {
      q.push(&node);
    }
  }

  size_t visited = 0;

  while(!q.empty()) {
    
    int level_length = static_cast<int>(q.size());
    level_list.emplace_back();
    level_list.back().reserve(level_length);

    for(int i = 0; i < level_length; i++) {
      Node* cur = q.front(); q.pop();
      cur->_lid = static_cast<int>(level_list.back().size());
      level_list.back().push_back(cur); 
      cur->_topo_id = visited++;

      for(auto fanout : cur->_fanouts) {
        Node* fanout_node = fanout->_to;
        if(--indegrees[fanout_node->_id] == 0) {
          q.push(fanout_node);
        }
      }
    }
  }

  if(visited != _nodes.size()) {
    throw std::runtime_error("The DAG has a cycle");
  }

  return level_list;
}

void Graph::partition_cudaflow(size_t num_streams) {

  // TODO: instead of reset the reconstructed graph, do it incrementally
  int id = 0;
  for(auto& node : _nodes) {
    node._id = id++;
    node._topo_id = -1;
    node._level = -1;
    node._lid = -1;
    node._sm = -1;
    node._reconstructed_fanins.clear();
    node._reconstructed_fanouts.clear();
  }

  // get level list 
  // assign lid to each node
  std::vector<std::vector<Node*>> level_list = _get_level_list(); 

  // use list to store nodes for each stream
  std::vector<std::list<Node*>> streams(num_streams);

  auto start = std::chrono::steady_clock::now();
  for(auto& level : level_list) {
    for(auto node : level) {
      int stream_id_cur = (node->_lid) % num_streams; 
      Node* last_assign = NULL; // "last" predecessor in the same stream 
                                // stream_id_prev to build dependency edge
      for(auto fanin : node->_fanins) {
        Node* predecessor = fanin->_from; 
        int stream_id_prev = (predecessor->_lid) % num_streams;
        if(stream_id_prev == node->_sm) {
          if(!last_assign || (last_assign && last_assign->_topo_id < predecessor->_topo_id)) {
            last_assign = predecessor;
          }
        }
        else if(stream_id_prev != stream_id_cur) {
          predecessor->_reconstructed_fanouts.push_back(node);
          node->_reconstructed_fanins.push_back(predecessor);
        }
      }
      if(last_assign) {
        last_assign->_reconstructed_fanouts.push_back(node);
        node->_reconstructed_fanins.push_back(last_assign);
      }
      streams[stream_id_cur].push_back(node);
      for(auto fanout : node->_fanouts) {
        Node* successor = fanout->_to;
        int stream_id_suc = (successor->_lid) % num_streams;
        if(stream_id_suc != stream_id_cur) {
          successor->_sm = stream_id_cur;
        }
      }
    }
  }

  // for nodes in the same streams, connect them as a linear chain
  for(auto list : streams) {
    for(auto it = list.begin(); it != list.end(); it++) {
      auto next = std::next(it);
      if(next != list.end()) {
        (*it)->_reconstructed_fanouts.push_back((*next));
        (*next)->_reconstructed_fanins.push_back((*it));
      }
    }
  }

  auto end = std::chrono::steady_clock::now();
  size_t partition_runtime = std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();
  _incre_partition_runtime_with_cudaflow_partition += partition_runtime;

  // if(!is_cudaflow_partition_share_same_topo_order()) {
  //   throw std::runtime_error("they do not share same topological order.\n");
  // }

  /*
  // dump graph to check
  tf::Taskflow taskflow;
  tf::Executor executor;

  auto start = std::chrono::steady_clock::now();
  for(auto& node : _nodes) {
    node._task = taskflow.emplace([this]() {
    }).name(node._name);
  }

  for(auto& node : _nodes) {
    for(auto successor : node._reconstructed_fanouts) {
      node._task.precede(successor->_task);
    }
  }

  taskflow.dump(std::cout);
  */
}

bool Graph::is_cudaflow_partition_share_same_topo_order() {

  // store the union graph of two DAGs as adjacent list
  std::vector<std::vector<int>> adj(_nodes.size());
  std::vector<int> indegrees(_nodes.size(), 0);

  // add original DAG to adj
  for(auto& node : _nodes) {
    indegrees[node._id] = node._fanins.size();
    for(auto fanout : node._fanouts) {
      Node* fanout_node = fanout->_to;
      adj[node._id].push_back(fanout_node->_id);
    }
  }

  // add cudaflow partitioned DAG to adj
  // here we increment the indegrees and add more edges
  // there could be duplicate edges in union graph
  // but the topological sort can handle this
  for(auto& node : _nodes) {
    indegrees[node._id] += node._reconstructed_fanins.size();
    for(auto fanout_node : node._reconstructed_fanouts) {
      adj[node._id].push_back(fanout_node->_id);
    }
  }

  // run topological sort to check if union graph is acyclic
  std::queue<int> q;
  for(int i = 0; i < static_cast<int>(_nodes.size()); i++) {
    if(indegrees[i] == 0) {
      q.push(i);
    }
  }

  size_t visited = 0;
  while(!q.empty()) {
    
    int cur = q.front();
    q.pop();
    visited++;

    for(int successor : adj[cur]) {
      if(--indegrees[successor] == 0) {
        q.push(successor);
      }
    }
  }

  return (visited == _nodes.size());
}

void Graph::run_graph_cudaflow_partition(size_t matrix_size, size_t num_streams) { // num_streams = max_parallelism

  partition_cudaflow(num_streams);

  _taskflow.clear();

  auto start1 = std::chrono::steady_clock::now();
  for(auto& node : _nodes) {
    node._task = _taskflow.emplace([this, matrix_size, &node]() {
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
    for(auto fanout_node : node._reconstructed_fanouts) {
      node._task.precede(fanout_node->_task);
    }
  }
  auto end1 = std::chrono::steady_clock::now();
  size_t construct_runtime = std::chrono::duration_cast<std::chrono::microseconds>(end1-start1).count();
  _incre_construct_runtime_with_cudaflow += construct_runtime;

  auto start = std::chrono::steady_clock::now();
  _executor.run(_taskflow).wait();
  auto end = std::chrono::steady_clock::now();
  size_t taskflow_runtime = std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();
  _incre_runtime_with_cudaflow_partition += taskflow_runtime;

}

void Graph::partition_cudaflow_incremental(size_t num_streams) {

}

std::vector<Node*> Graph::_get_topo_order_dfs() {

  std::vector<bool> visited(_nodes.size(), false);

  std::vector<Node*> topo_dfs;

  // Get DFS topological sequence in reverse
  auto dfs = [&](auto&& self, Node* node, std::vector<bool>& visited, std::vector<Node*>& topo) -> void {
    visited[node->_id] = true;
    for(auto fanout : node->_fanouts) {
      Node* fanout_node = fanout->_to;
      if(!visited[fanout_node->_id]) {
        self(self, fanout_node, visited, topo);
      }
    } 
    topo.push_back(node);
  };

  for(auto& node : _nodes) {
    if(node._fanins.size() == 0) {
      dfs(dfs, &node, visited, topo_dfs);
    }
  }

  std::reverse(topo_dfs.begin(), topo_dfs.end());

  return topo_dfs;
}

bool Graph::_has_original_edge(Node* from, Node* to) const {
  return from->_fanout_set.find(to) != from->_fanout_set.end();
} 

void Graph::generate_topo_order() {

  auto start = std::chrono::steady_clock::now();
  std::vector<Node*> topo_dfs = _get_topo_order_dfs(); 
  auto end = std::chrono::steady_clock::now();

  _generate_topo_order_time += std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();

}

bool Graph::process_backward_edges() {

  _num_backward_edges += _backward_edges.size();

  auto start = std::chrono::steady_clock::now();
  while(!_backward_edges.empty()) {
    Edge* e = _backward_edges.front();
    _backward_edges.pop();

    // std::cout << "backward edge: " << e->_from->_name << "(" << e->_from->_pos << ") -> "
    //                                << e->_to->_name << "(" << e->_to->_pos << ")\n";

    Node* from = e->_from;
    Node* to = e->_to;

    // not backward anymore
    if(from->_pos < to->_pos) {
      continue;
    }

    bool has_cycle = _restricted_dfs(from, to);
    if(has_cycle) {
      return false;
    }

    std::list<Node*> moved;

    auto stop = std::next(from->_topo_it);
    for(auto it = to->_topo_it; it != stop; ) {
      auto cur = it++;
      Node* n = *cur;
      
      if(n->_dfs_tag == _cur_dfs_tag) {
        // move the dfs reachable nodes from _topo_nodes to moved
        moved.splice(moved.end(), _topo_nodes, cur);
      }
    }

    // std::cout << "moved: ";
    // for(auto n : moved) {
    //   std::cout << n->_name << " ";
    // }
    // std::cout << "\n";

    // save moved size and old right boundary before insertion
    size_t moved_size = moved.size();
    auto right_it = std::next(from->_topo_it);
    Node* right = (right_it == _topo_nodes.end())? nullptr : *right_it;

    _topo_nodes.splice(std::next(from->_topo_it), moved);

    if(right) {
      _relabel_after_from_until(from, right, moved_size);
    }
    else {
      _relabel_after_left_to_end(from, moved_size);
    }

    // if(!check_topo_iterators_and_pos()) {
    //   throw std::runtime_error("topo_itr and pos wrong");
    //   return false;
    // }

  }
  auto end = std::chrono::steady_clock::now();

  _process_backward_edge_time += std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();

  // if(!is_topo_nodes_valid()) {
  //   return false;
  // }

  return true;

}

bool Graph::process_backward_edges_taskflow() { 

  _num_backward_edges += _backward_edges.size();

  while(!_backward_edges.empty()) {
    Edge* e = _backward_edges.front();
    _backward_edges.pop();

    Node* from = e->_from;
    Node* to = e->_to;

    // not backward anymore
    if(from->_pos < to->_pos) {
      continue;
    }

    bool has_cycle = _restricted_dfs(from, to);
    if(has_cycle) {
      return false;
    }

    // Before move around topo_nodes,
    // record left_update_bound and right_update_bound for _taskflow sequence
    // only consider update _task and _linked_to_is_actual for nodes in [left_update_bound, right_update_bound] 
    // Take a snapshot of from and to before _topo_nodes changes
    bool to_is_begin = to->_topo_it == _topo_nodes.begin();
    bool from_is_end = std::next(from->_topo_it) == _topo_nodes.end();
    auto lit = std::prev(to->_topo_it);
    auto rit = std::next(from->_topo_it);

    std::list<Node*> moved;

    auto stop = std::next(from->_topo_it);
    for(auto it = to->_topo_it; it != stop; ) {
      auto cur = it++;
      Node* n = *cur;
      
      if(n->_dfs_tag == _cur_dfs_tag) {
        // move the dfs reachable nodes from _topo_nodes to moved
        moved.splice(moved.end(), _topo_nodes, cur);
      }
    }

    // save moved size and old right boundary before insertion
    size_t moved_size = moved.size();
    auto right_it = std::next(from->_topo_it);
    Node* right = (right_it == _topo_nodes.end())? nullptr : *right_it;

    _topo_nodes.splice(std::next(from->_topo_it), moved);

    if(right) {
      _relabel_after_from_until(from, right, moved_size);
    }
    else {
      _relabel_after_left_to_end(from, moved_size);
    }

    // After _topo_nodes changes, fix lit and rit
    lit = (to_is_begin) ? _topo_nodes.begin() : lit;
    rit = (from_is_end) ? std::prev(_topo_nodes.end()) : rit;

    Node* left_update_bound = *lit;
    Node* right_update_bound = *rit;

    // std::cerr << "process edge: " << from->_name << " -> " << to->_name << "\n";
    // std::cerr << "current _topo_nodes = ";
    // for(auto it = _topo_nodes.begin(); it != _topo_nodes.end(); it++) {
    //   std::cout << (*it)->_name << " ";
    // }
    // std::cout << "\n";
    // std::cerr << "left_bound: " << left_update_bound->_name 
    //           << " right_bound: " << right_update_bound->_name << "\n"; 

    // process taskflow sequence based on updated _topo_nodes
    _update_taskflow_sequence(left_update_bound, right_update_bound);

  }

  return true;

}

void Graph::_update_taskflow_sequence(Node* left_update_bound, Node* right_update_bound) {

  // Traverse [left_update_bound, right_update_bound] in _topo_nodes.
  /*
    For each node in [left_update_bound, right_update_bound],
    -> check if cur is the last one, 
        -> yes, then check if cur->_linked_to is null
            -> no, then check if cur and cur->_linked_to has actual edge 
              (in this case, they should not, so no need to check actually)
                -> no, remove this edge in taskflow
           set cur->_linked_to = null, break
        -> no, then continue...
      
    continue...
    -> check if cur->_linked_to == next,
        -> yes, no need to update _task and _linked_to_is_actual
        -> no, update _task and _linked_to_is_actual:
               cur->_linked_to_is_actual = _has_original_edge(cur, cur->_linked_to)  
               if _linked_to_is_actual = false, cur->_task.remove_successors(cur->_linked_to->_task)
               cur->_linked_to = next
  */
  for(auto it = left_update_bound->_topo_it; it != std::next(right_update_bound->_topo_it); it++) {
    Node* cur = *it;
    if(it == std::prev(_topo_nodes.end())) {
      // if(!_has_original_edge(cur, cur->_linked_to) && cur->_linked_to != nullptr) {
      //   cur->_task.remove_successors(cur->_linked_to->_task);
      // }
      cur->_linked_to = nullptr;
      break;
    }
    Node* next = *(std::next(it)); 
    if(cur->_linked_to != next) {
      if(!_has_original_edge(cur, cur->_linked_to) &&
         cur->_linked_to != nullptr) { // have to do the recheck 
                                       // cuz user may add edge after you have build up the _topo_nodes
        cur->_task.remove_successors(cur->_linked_to->_task);
      }
      cur->_linked_to = next;
      cur->_linked_to_is_actual = _has_original_edge(cur, cur->_linked_to);
    }
  }

}

bool Graph::_restricted_dfs(Node* from, Node* to) {

  ++_cur_dfs_tag; 

  uint64_t left = to->_pos;
  uint64_t right = from->_pos;

  std::stack<Node*> st;
  st.push(to);

  while(!st.empty()) {

    Node* cur = st.top();
    st.pop();

    if(cur->_dfs_tag == _cur_dfs_tag) {
      continue; // has been visited
    }

    cur->_dfs_tag = _cur_dfs_tag;

    if(cur == from) { 
      return true; // cycle detected
    }

    for(Edge* fanout : cur->_fanouts) {

      Node* nxt = fanout->_to;

      if(nxt->_pos < left || nxt->_pos > right) {
        continue;
      }

      if(nxt->_dfs_tag != _cur_dfs_tag) {
        st.push(nxt);
      }
    }

  }

  return false;

}

bool Graph::check_topo_iterators_and_pos() const {
  for (auto it = _topo_nodes.begin(); it != _topo_nodes.end(); ++it) {
    if((*it)->_topo_it != it) {
      return false;
    }
    auto next = std::next(it);
    if(next != _topo_nodes.end()) {
      if((*it)->_pos >= (*next)->_pos) {
        return false;
      }
    }
  }
  return true;
}

bool Graph::is_topo_nodes_valid() const {

  std::unordered_map<Node*, size_t> pos;

  size_t idx = 0;
  for(auto node : _topo_nodes) {
    pos[node] = idx++;
  }
  for(auto from : _topo_nodes) {
    for(auto fanout : from->_fanouts) {
      Node* to = fanout->_to;
      if(pos[from] >= pos[to]) {
        return false;
      }
    }
  }

  return true;

}

bool Graph::validate_modify_edge() const {
  return check_topo_iterators_and_pos() && is_topo_nodes_valid();
}

void Graph::_relabel_after_from_until(Node* left, Node* right, size_t k) {

  if(k == 0) {
    return;
  }

  uint64_t left_pos  = left->_pos;
  uint64_t right_pos = right->_pos;

  if(right_pos <= left_pos || right_pos - left_pos <= k) {
    _relabel_full();
    return;
  }

  uint64_t step = (right_pos - left_pos) / static_cast<uint64_t>(k + 1);

  if(step == 0) {
    _relabel_full();
    return;
  }

  auto it = std::next(left->_topo_it);
  uint64_t prev = left_pos;

  for(size_t i = 0; i < k; ++i, ++it) {
    uint64_t cur = left_pos + step * static_cast<uint64_t>(i + 1);

    if(cur <= prev || cur >= right_pos) {
      _relabel_full();
      return;
    }

    (*it)->_pos = cur;
    prev = cur;
  }
}

void Graph::_relabel_after_left_to_end(Node* left, size_t k) {

  if(k == 0) {
    return;
  }

  static constexpr uint64_t GAP = 1ull << 20;

  uint64_t pos = left->_pos;

  auto it = std::next(left->_topo_it);

  for(size_t i = 0; i < k; ++i, ++it) {
    if(UINT64_MAX - pos < GAP) {
      _relabel_full();
      return;
    }

    pos += GAP;
    (*it)->_pos = pos;
  }
}

void Graph::_relabel_full() {

  uint64_t pos = 0;
  for(auto it = _topo_nodes.begin(); it != _topo_nodes.end(); it++) {
    Node* node = *it;
    node->_pos = pos;
    pos += 1024;
  }

}

void Graph::_construct_taskflow_linear_chain(size_t matrix_size) {

  // In IncrementalPartition mode, we have constructed the task in constructor 
  // emplace task
  // for(auto& node : _nodes) {
  //   node._task = _taskflow.emplace([this, matrix_size, &node]() {
  //     // std::this_thread::sleep_for(std::chrono::nanoseconds(task_runtime));
  //     size_t N = matrix_size;
  //     size_t M = matrix_size;
  //     size_t K = matrix_size;
  //     std::vector<int> A(N*K, 1);
  //     std::vector<int> B(K*M, 2);
  //     std::vector<int> C(N*M);
  //     for(size_t n=0; n<N; n++) {
  //       for(size_t m=0; m<M; m++) {
  //         int temp = 0;
  //         for(size_t k=0; k<K; k++) {
  //           temp += A[n*K + k] * B[k*M + m];
  //         }
  //         C[n*M + m] = temp;
  //       }
  //     }
  //   }).name(node._name);
  // }

  // We have also connected the dependencies in constructor
  // connect original dependencies
  // for(auto& node : _nodes) {
  //   for(auto fanout : node._fanouts) {
  //     node._task.precede(fanout->_to->_task);
  //   }
  // }

  // connect extra dependencies as a linear chain based on _breakable nodes 
  for(auto node_ptr : _breakable_nodes) {
    auto next_it = std::next(node_ptr->_topo_it);
    Node* next = *next_it;
    node_ptr->_task.precede(next->_task);
  }

}

void Graph::_build_breakable_nodes() {

  _breakable_nodes.clear();

  if(_topo_nodes.size() <= 1) {
    return;
  }

  // collect all legal break points 
  for(auto it = _topo_nodes.begin(); it != _topo_nodes.end(); ++it) {

    auto next_it = std::next(it);

    // last node has no successor, so it cannot be a break point
    if(next_it == _topo_nodes.end()) {
      break;
    }

    Node* u = *it;
    if(!u->_linked_to_is_actual) {
      _breakable_nodes.push_back(u);
    }
  }
}

std::vector<Node*> Graph::_select_breakable_nodes(size_t cur_parallelism) const {

  std::vector<Node*> selected;

  size_t achievable = 1 + _breakable_nodes.size();
  cur_parallelism = std::min(cur_parallelism, achievable);

  if(cur_parallelism <= 1) {
    return selected;
  }

  selected.reserve(cur_parallelism - 1);

  for(size_t i = 1; i < cur_parallelism; ++i) {
    size_t j = (i * _breakable_nodes.size()) / cur_parallelism;
    selected.push_back(_breakable_nodes[j]);
  }

  return selected;
}

bool Graph::_need_to_rebuild_breakable_nodes() const {

  // Check each node in _breakable_nodes
  // 1. If a node is the first or last node in _topo_nodes, rebuild. 
  // 2. If a node->_linked_to_is_actual = true, rebuild
  // 3. Adjacent nodes in _breakable_nodes must have at least 10 space in _pos,
  //    i.e. next->_pos >= cur->_pos + 10; otherwise rebuild.
  for(auto node_ptr : _breakable_nodes) {
    if(node_ptr->_topo_it == _topo_nodes.begin() || node_ptr->_topo_it == std::prev(_topo_nodes.end())) {
      return true;
    }
  }

  for(auto node_ptr : _breakable_nodes) {
    if(node_ptr->_linked_to_is_actual) {
      return true;
    }
  }

  if(_breakable_nodes.size() > 1) {
    for(size_t i = 0; i < _breakable_nodes.size() - 1; i++) {
      Node* cur = _breakable_nodes[i];
      Node* next = _breakable_nodes[i + 1]; 
      if(cur->_pos + 10 > next->_pos) { // use addition to be safe for uint64
        return true;
      }
    }
  }

  return false;
}

void Graph::run_graph_incre_partition(size_t matrix_size, size_t cur_parallelism, size_t max_parallelism) {  

  // Step 1 (only done once in the first complete run): 
  //   Get the break point vectors based on max_parallelism
  if(_first_run) {
    _build_breakable_nodes();
  }
  _first_run = false;

  // Step 2: 
  //   Process backward edges
  //   Check if the updated topological sequence will invalid _breakable_nodes, if so, rebuild _breakable_nodes
  process_backward_edges_taskflow();
  if(_need_to_rebuild_breakable_nodes()) {
    _build_breakable_nodes();
  }

  _critical_path_length_original += _get_critical_path_length_taskflow();

  // Step 3:
  //   Make taskflow linear chain with the latest breakable nodes
  for(auto node_ptr : _breakable_nodes) {
    node_ptr->_task.precede(node_ptr->_linked_to->_task);
  }

  // Step 4:
  //   Before each run, select the breakable nodes from _breakable_nodes based on cur_parallelism
  std::vector<Node*> selected_breakable_nodes = _select_breakable_nodes(cur_parallelism);

  // Step 5:
  //   Break taskflow linear chain based on selected_breakable_nodes
  for(auto node_ptr : selected_breakable_nodes) {
    auto next_it = std::next(node_ptr->_topo_it);
    Node* next = *next_it;
    node_ptr->_task.remove_successors(next->_task);
  }

  _critical_path_length_constrained += _get_critical_path_length_taskflow();

  // Step 6:
  //   Run taskflow
  auto start = std::chrono::steady_clock::now();
  _executor.run(_taskflow).wait();
  auto end = std::chrono::steady_clock::now();
  _incre_pasta_taskflow_runtime += std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();

  // Step 7:
  //   Remove all the extra dependencies from taskflow to ensure a clean state for next iteration
  for(auto node_ptr : _breakable_nodes) {
    node_ptr->_task.remove_successors(node_ptr->_linked_to->_task);
  }
}

void Graph::run_graph_incre_partition_seq(size_t matrix_size, size_t cur_parallelism, size_t max_parallelism) {

  // Step 1 (only done once in the first complete run): 
  //   Get the break point vectors based on max_parallelism
  if(_first_run) {
    _build_breakable_nodes();
  }
  _first_run = false;

  // Step 2: 
  //   Process backward edges
  //   Check if the updated topological sequence will invalid _breakable_nodes, if so, rebuild _breakable_nodes
  process_backward_edges_taskflow();
  if(_need_to_rebuild_breakable_nodes()) {
    _build_breakable_nodes();
  }

  // Step 3:
  //   Make taskflow linear chain with the latest breakable nodes
  for(auto node_ptr : _breakable_nodes) {
    node_ptr->_task.precede(node_ptr->_linked_to->_task);
  }

  // Step 4:
  //   Before each run, select the breakable nodes from _breakable_nodes based on cur_parallelism
  std::vector<Node*> selected_breakable_nodes = _select_breakable_nodes(cur_parallelism);

  // Remove this step to always run taskflow as a linear chain
  // // Step 5:
  // //   Break taskflow linear chain based on selected_breakable_nodes
  // for(auto node_ptr : selected_breakable_nodes) {
  //   auto next_it = std::next(node_ptr->_topo_it);
  //   Node* next = *next_it;
  //   node_ptr->_task.remove_successors(next->_task);
  // }

  _critical_path_length_constrained += _get_critical_path_length_taskflow();

  // Step 6:
  //   Run taskflow
  auto start = std::chrono::steady_clock::now();
  _executor.run(_taskflow).wait();
  auto end = std::chrono::steady_clock::now();
  _incre_pasta_taskflow_runtime += std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();

  // Step 7:
  //   Remove all the extra dependencies from taskflow to ensure a clean state for next iteration
  for(auto node_ptr : _breakable_nodes) {
    node_ptr->_task.remove_successors(node_ptr->_linked_to->_task);
  }
}  

void Graph::print_topo_order() const {
  
  std::cout << "_topo_nodes = \n";
  for(auto it = _topo_nodes.begin(); it != _topo_nodes.end(); it++) {
    std::cout << (*it)->_name << " ";
  }
  std::cout << "\n";
}

void Graph::remove_actual_edge() {

  // remove_edge(&(*(_edges.begin())), RunMode::IncrementalPartition);
  int count = 0;
  for(auto it = _edges.begin(); it != _edges.end(); it++) {
    if(count == 7) {
      remove_edge(&(*it), RunMode::IncrementalPartition);
      break;
    }
    count++;
  }
}

void Graph::add_backward_edge() {

  // topo sequence: 0 7 8 9 4 5 6 1 2 3 10
  // original breakable nodes: 9 and 6
  // add one from 5 to 7 (bedge)
  // add one from 9 to 6
  // after should be just 6
  size_t pos = 0;
  Node* from = nullptr;
  Node* to = nullptr;
  for(auto it = _topo_nodes.begin(); it != _topo_nodes.end(); it++) {
    if(pos == 1) {
      to = *it;
    }
    if(pos == 5) {
      from = *it; 
    }
    pos++;
  }

  insert_edge(from, to, RunMode::IncrementalPartition);

  pos = 0;
  for(auto it = _topo_nodes.begin(); it != _topo_nodes.end(); it++) {
    if(pos == 6) {
      to = *it;
    }
    if(pos == 3) {
      from = *it; 
    }
    pos++;
  }

  insert_edge(from, to, RunMode::IncrementalPartition);
}

bool Graph::is_taskflow_topo_consistent() {

  /*
   * Actually run taskflow with edges changes
   */
  size_t max_parallelism = 8;
  size_t cur_parallelism = 2;
  size_t matrix_size = 1;

  // Step 1 (only done once in the first complete run): 
  //   Get the break point vectors based on max_parallelism
  if(_first_run) {
    _build_breakable_nodes();
  }
  _first_run = false;

  // Step 2: 
  //   Process backward edges
  //   Check if the updated topological sequence will invalid _breakable_nodes, if so, rebuild _breakable_nodes
  process_backward_edges_taskflow();
  if(_need_to_rebuild_breakable_nodes()) {
    // Before building new breakable nodes, 
    // I have removed all the extra dependencies at the end of last iteration  
    _build_breakable_nodes();
  }

  // CHECK: After process backward edge, check _linked_to
  if(!is_linked_to_match_topo()) {
    return false;
  }

  // CHECK: We also need to verify if _breakable_nodes is correct
  if(!is_breakable_nodes_complete()) {
    return false;
  }

  // CHECK: Check if the number of dependents & successors are the same
  for(auto& node : _nodes) {
    if((node._fanouts.size() != node._task.num_successors()) ||
       (node._fanins.size() != node._task.num_predecessors())) {
      return false;
    }
  }

  // Step 3:
  //   Make taskflow linear chain with the latest breakable nodes  
  for(auto node_ptr : _breakable_nodes) {
    node_ptr->_task.precede(node_ptr->_linked_to->_task);
  }

  // CHECK: Check if taskflow is a linear chain
  if(!is_taskflow_linear_chain()) {
    return false;
  }

  // Step 4:
  //   Before each run, select the breakable nodes from _breakable_nodes based on cur_parallelism
  std::vector<Node*> selected_breakable_nodes = _select_breakable_nodes(cur_parallelism);

  // Step 5:
  //   Break taskflow linear chain based on selected_breakable_nodes
  for(auto node_ptr : selected_breakable_nodes) {
    auto next_it = std::next(node_ptr->_topo_it);
    Node* next = *next_it;
    node_ptr->_task.remove_successors(next->_task);
  }

  // Step 6:
  //   Run taskflow
  _executor.run(_taskflow).wait();

  // Step 7:
  //   Remove all the extra dependencies from taskflow to ensure a clean state for next iteration
  for(auto node_ptr : _breakable_nodes) {
    node_ptr->_task.remove_successors(node_ptr->_linked_to->_task);
  }

  return true;
}

bool Graph::is_taskflow_linear_chain() {

  std::vector<std::vector<Node*>> level_list = _get_taskflow_level_list();

  size_t index = 0;
  for(auto level : level_list) {
    // std::cout << "level " << index << ": ";
    // for(auto node_ptr : level) {
    //   std::cout << node_ptr->_name << " ";
    // }
    // std::cout << "\n";
    // index++;
    if(level.size() > 1) {
      return false;
    }
  }

  return true;
}

size_t Graph::_get_max_parallelism_taskflow() {

  std::vector<std::vector<Node*>> level_list = _get_taskflow_level_list();

  size_t parallelism = 0;
  for(auto level : level_list) {
    parallelism = std::max(parallelism, level.size());
  }

  return parallelism;
}

size_t Graph::_get_critical_path_length_taskflow() {

  std::vector<tf::Task> tasks;
  tasks.reserve(_taskflow.num_tasks());

  // task name -> index
  std::unordered_map<std::string, size_t> task_index;
  task_index.reserve(_taskflow.num_tasks());

  _taskflow.for_each_task([&](tf::Task t) {
    task_index[t.name()] = tasks.size();
    tasks.push_back(t);
  });

  if(tasks.empty()) {
    return 0;
  }

  // indegree for topological traversal
  std::vector<size_t> indeg(tasks.size(), 0);
  for(size_t i = 0; i < tasks.size(); ++i) {
    indeg[i] = tasks[i].num_predecessors();
  }

  // dp[i] = longest path length ending at task i
  // here length is measured in number of nodes on the path
  std::vector<size_t> dp(tasks.size(), 1);

  std::queue<size_t> q;
  for(size_t i = 0; i < tasks.size(); ++i) {
    if(indeg[i] == 0) {
      q.push(i);
    }
  }

  size_t visited = 0;
  size_t longest = 0;

  while(!q.empty()) {
    size_t u = q.front();
    q.pop();
    ++visited;

    longest = std::max(longest, dp[u]);

    tasks[u].for_each_successor([&](tf::Task succ) {
      auto sit = task_index.find(succ.name());
      if(sit == task_index.end()) {
        throw std::runtime_error(
          "cannot find successor index for task " + succ.name()
        );
      }

      size_t v = sit->second;

      // relax longest path
      dp[v] = std::max(dp[v], dp[u] + 1);

      if(--indeg[v] == 0) {
        q.push(v);
      }
    });
  }

  if(visited != tasks.size()) {
    throw std::runtime_error("Taskflow graph is not a DAG");
  }

  return longest;
}

bool Graph::is_linked_to_match_topo() {

  for(auto it = _topo_nodes.begin(); it != _topo_nodes.end(); it++) {
    Node* cur = *it;
    if(it != std::prev(_topo_nodes.end())) {
      Node* next = *(std::next(it));
      if(cur->_linked_to != next) {
        return false;
      }
    }
    else {
      if(cur->_linked_to != nullptr) {
        return false;
      }
    }
  }

  for(auto& node : _nodes) {
    if(node._linked_to != nullptr) {
      if(node._linked_to_is_actual != _has_original_edge(&node, node._linked_to)) {
        return false;
      }
    }
    else {
      // If this node is the last one and the flag is still on, then wrong
      if(node._linked_to_is_actual == true) {
        return false;
      }
    }
  }

  return true;
}

bool Graph::is_breakable_nodes_complete() {

  // Ground truth
  std::set<Node*> breakable_node_set_truth;

  // collect all legal break points 
  for(auto it = _topo_nodes.begin(); it != _topo_nodes.end(); ++it) {

    auto next_it = std::next(it);

    // last node has no successor, so it cannot be a break point
    if(next_it == _topo_nodes.end()) {
      break;
    }

    Node* u = *it;
    if(!u->_linked_to_is_actual) {
      breakable_node_set_truth.insert(u);
    }
  }


  std::set<Node*> breakble_node_set;
  for(auto node_ptr : _breakable_nodes) {
    breakble_node_set.insert(node_ptr);
  }

  return (breakable_node_set_truth.size() == breakble_node_set.size());

}

} // end of namespace pasta


























