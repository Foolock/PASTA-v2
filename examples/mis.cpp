#include <algorithm>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

// build an undirected simple graph (no self loop, no multi-edges)
// we add each possible edge  (u, v) with probability p
std::vector<std::vector<int>> make_undirected_graph(int n, double p) {
  std::mt19937 rng(42);
  std::bernoulli_distribution coin(p);

  std::vector<std::vector<int>> g(n);
  for(int u = 0; u < n; u++) {
    for(int v = u + 1; v < n; v++) {
      if(coin(rng)) {
        g[u].push_back(v);
        g[v].push_back(u);
      }   
    }
  }

  return g;
}

// count number of edges in an undirected adjacency list (each edge appears twice)
long long count_edges_undirected(const std::vector<std::vector<int>>& g) {
  long long sum = 0;
  for(auto& list : g) {
    sum += (long long)list.size();
  }

  return sum / 2; 
}

// greedy maximal independent set (MIS):
// iterate vertices in fixed order, pick an eligible vertex
// then mark it and its neighbors ineligible
std::vector<int> mis_greedy(const std::vector<std::vector<int>>& g) {
  int n = (int)g.size();
  std::vector<char> eligible(n, true);
  std::vector<char> in_mis(n, false);

  for(int u = 0; u < n; u++) {
    if(!eligible[u]) continue;

    // pick u
    in_mis[u] = true;

    // remove u and its neighbors from eligibility
    eligible[u] = false;
    for (int v : g[u]) {
      eligible[v] = false;
    }
  }

  std::vector<int> mis;
  for(int u = 0; u < n; u++) {
    if (in_mis[u]) {
      mis.push_back(u);
    }
  }

  return mis;
}

// in greedy MIS, if a vertex is picked, all its neighbors are permanently blocked.
// so early picks matter a lot. randomizing the order reduces the chance that a "bad" 
// early choice ruins the rest.
// random greedy MIS:
// shuffle the vertex order 
// run the same greedy rule as the simple sequential version 
std::vector<int> mis_random_greedy(const std::vector<std::vector<int>>& g) {
  int n = (int)g.size();

  // build a random vertex order
  std::mt19937 rng(42);
  std::vector<int> order(n);
  std::iota(order.begin(), order.end(), 0);

  // run greedy algorithm
  std::vector<char> eligible(n, true);
  std::vector<char> in_mis(n, false);
  for(int u : order) {
    if(!eligible[u]) continue;
    eligible[u] = false;
    in_mis[u] = true;
    for(int v : g[u]) {
      eligible[v] = false;
    }
  }

  std::vector<int> mis;
  for(int u = 0; u < n; u++) {
    if(in_mis[u]) mis.push_back(u);
  }

  return mis;
}


// Luby's algorithm (PRAM-style randomized MIS), sequential simulation
// Returns a maximal independent set.
// 
// High-level (per round):
// 1) Each active vertex picks a random priority.
// 2) A vertex is selected if it has higher prioity than all active neighbors
//    (if there is a tie, use vertex id to determine).
// 3) Add selected vertices to MIS; deactivate them and all their neighbors
// Repeat until no active vertices remain.
std::vector<int> mis_luby(const std::vector<std::vector<int>>& g) {
  int n = (int)g.size();
  std::mt19937 rng(42);

  std::vector<char> active(n, true);
  std::vector<char> in_mis(n, false);
  std::vector<uint32_t> priority(n, 0);

  int active_count = n;
  while(active_count > 0) {
    // 1) Random priorities for active vertices
    for(int u = 0; u < n; u++) {
      if(active[u]) {
        priority[u] = rng();
      }
    }

    // 2) Local maxima selection
    //    Choose the one neighbor with the highest priority
    std::vector<char> selected(n, false);
    for(int u = 0; u < n; u++) {
      if(!active[u]) continue;
      
      bool win = true;
      for(int v : g[u]) {
        if(!active[v]) continue;

        // If neighbor has strictly higher priority, u loses
        // If equal priority, break ties by id (higher id wins here)
        if(priority[v] > priority[u] || (priority[v] == priority[u] && v > u)) {
          win = false;
          break;
        }
      }
      if(win) selected[u] = true; 
    }

    // 3) Deactivate selected vertices and their neighbors
    std::vector<int> to_deactivate;
    to_deactivate.reserve(n);

    for(int u = 0; u < n; u++) {
      if(!selected[u]) continue;

      in_mis[u] = true;

      if(active[u]) to_deactivate.push_back(u);
      for(int v : g[u]) {
        if(active[v]) to_deactivate.push_back(v);
      }
    }

    // ???
    // This is used to remove the duplicated nodes in to_deactivate
    // since two nodes might have same neighbors
    std::sort(to_deactivate.begin(), to_deactivate.end());
    to_deactivate.erase(std::unique(to_deactivate.begin(), to_deactivate.end()),
                        to_deactivate.end());

    for(int u : to_deactivate) {
      if(active[u]) {
        active[u] = false;
        --active_count; 
      }
    }

  }

  std::vector<int> mis;
  for(int u = 0; u < n; u++) {
    if(in_mis[u]) mis.push_back(u);
  }
  return mis;
}


bool is_independent_set(const std::vector<std::vector<int>>& g, const std::vector<int>& S) {
  int n = (int)g.size();
  std::vector<char> in(n, false);
  for(int u : S) {
    in[u] = true;
  }

  // the two neighboring nodes should not be in MIS at the same time
  for(int u : S) {
    for(int v : g[u]) {
      if(in[v]) {
        return false;
      }
    }
  }

  return true;
}

// maximal: you can't add any other vertex without breaking independence
// equivalent check: every vertex not in S has at least one neighbor in S
bool is_maximal_independent_set(const std::vector<std::vector<int>>& g, const std::vector<int>& S) {
  int n = (int)g.size();
  std::vector<char> in(n, false);
  for(int u : S) {
    in[u] = true;
  }

  for(int u = 0; u < n; u++) {
    if(in[u]) continue;
    bool inS = false;
    for(int v : g[u]) {
      if(in[v]) {
        inS = true;
        break;
      } 
    }
    if(!inS) return false;
  }

  return true;
}

int main() {

  const int n = 100;
  const double p = 0.1;

  auto g = make_undirected_graph(n, p);
  long long m = count_edges_undirected(g);
  std::cout << "graph has " << n << " nodes, and " << m << " edges\n"; 

  auto S_greedy = mis_greedy(g);
  std::cout << "S_greedy has " << S_greedy.size() << " nodes\n"; 
  std::cout << "S_greedy is MIS? " << ((is_independent_set(g, S_greedy) && 
                                        is_maximal_independent_set(g, S_greedy))? "YES" : "NO")
                                   << "\n";

  auto S_random_greedy = mis_random_greedy(g);
  std::cout << "S_random_greedy has " << S_random_greedy.size() << " nodes\n"; 
  std::cout << "S_random_greedy is MIS? " << ((is_independent_set(g, S_random_greedy) && 
                                               is_maximal_independent_set(g, S_random_greedy))? "YES" : "NO")
                                          << "\n";

  auto S_luby = mis_luby(g);
  std::cout << "S_luby has " << S_luby.size() << " nodes\n"; 
  std::cout << "S_luby is MIS? " << ((is_independent_set(g, S_luby) && 
                                      is_maximal_independent_set(g, S_luby))? "YES" : "NO")
                                 << "\n";
}





















