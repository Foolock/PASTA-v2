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

  // Node node(name);
  int id = (int)_nodes.size();
  Node* node_ptr = &(_nodes.emplace_back(name));
  node_ptr->_node_satellite = --_nodes.end();
  node_ptr->_id = id;

  return node_ptr;
}

Edge* Graph::insert_edge(Node* from, Node* to) {

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

  return edge_ptr;
}

void Graph::remove_node(Node* node) {

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
  _nodes.erase(node->_node_satellite);
}

void Graph::remove_edge(Edge* edge) {

  Node* from = edge->_from;
  Node* to = edge->_to;

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

void Graph::remove_random_nodes(size_t N, std::mt19937& gen) {

  N = std::min(N, _nodes.size());
  if (N == 0) return;

  // collect pointers
  std::vector<Node*> cand;
  cand.reserve(_nodes.size());
  for (auto& n : _nodes) cand.push_back(std::addressof(n));

  std::shuffle(cand.begin(), cand.end(), gen);
  cand.resize(N);

  for (Node* p : cand) remove_node(p); // your existing internal removal

}

void Graph::remove_random_edges(size_t N, std::mt19937& gen) {

  N = std::min(N, _edges.size());
  if (N == 0) return;

  std::vector<Edge*> cand;
  cand.reserve(_edges.size());
  for (auto& e : _edges) cand.push_back(std::addressof(e));

  std::shuffle(cand.begin(), cand.end(), gen);
  cand.resize(N);

  for (Edge* p : cand) {
    remove_edge(p);
  }
}

size_t Graph::add_random_edges(size_t N, std::mt19937& gen, size_t max_tries_multiplier) {

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

    insert_edge(from, to);
    ++added;
  }

  return added;  // could be < N if graph is already dense
}

std::vector<Node*> Graph::add_random_nodes(size_t N, std::mt19937& gen, const std::string& name_prefix) {
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
    new_nodes.push_back(insert_node(name));
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
        insert_edge(ex, nn);      // existing -> new
      }
    } else {
      if (!has_edge(nn, ex)) {
        insert_edge(nn, ex);      // new -> existing
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
  size_t origin_taskflow_runtime = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();

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

  _taskflow.clear();
  _semaphore.reset(num_semaphore);

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

  auto start = std::chrono::steady_clock::now();
  _executor.run(_taskflow).wait();
  auto end = std::chrono::steady_clock::now();
  size_t taskflow_runtime = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
  _incre_runtime_with_semaphore += taskflow_runtime;

  // printf("For current iteration, taskflow runtime with #semaphores = %ld: %ld ms\n", num_semaphore, taskflow_runtime);
}

void Graph::dump_graph() {

  tf::Taskflow taskflow;
  tf::Executor executor;

  auto start = std::chrono::steady_clock::now();
  for(auto& node : _nodes) {
    node._task = taskflow.emplace([this]() {
    }).name(node._name);
  }

  for(auto& node : _nodes) {
    for(auto fanout : node._fanouts) {
      node._task.precede(fanout->_to->_task);
    }
  }

  taskflow.dump(std::cout);
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

  // std::vector<std::vector<Node*>> level_list = _get_level_list();

  // int level_id = 0;
  // for(auto level : level_list) {
  //   std::cout << "level " << level_id << ": ";
  //   for(auto node_ptr : level) {
  //     std::cout << node_ptr->_name << "(" << node_ptr->_lid << ") ";
  //   }
  //   std::cout << "\n";
  // }

  partition_cudaflow(2);
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

  auto start = std::chrono::steady_clock::now();
  _executor.run(_taskflow).wait();
  auto end = std::chrono::steady_clock::now();
  size_t taskflow_runtime = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
  _incre_runtime_with_cudaflow_partition += taskflow_runtime;

}

} // end of namespace pasta


























