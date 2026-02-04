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
  auto S_greedy = mis_greedy(g);
  auto S_random_greedy = mis_random_greedy(g);

  long long m = count_edges_undirected(g);

  std::cout << "graph has " << n << " nodes, and " << m << " edges\n"; 
  std::cout << "S_greedy has " << S_greedy.size() << " nodes\n"; 
  std::cout << "S_random_greedy has " << S_random_greedy.size() << " nodes\n"; 

  std::cout << "S_greedy is MIS? " << ((is_independent_set(g, S_greedy) && 
                                        is_maximal_independent_set(g, S_greedy))? "YES" : "NO")
                                   << "\n";

  std::cout << "S_random_greedy is MIS? " << ((is_independent_set(g, S_random_greedy) && 
                                               is_maximal_independent_set(g, S_random_greedy))? "YES" : "NO")
                                          << "\n";



}





















