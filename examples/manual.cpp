#include "pasta.hpp"

int main() {

  std::mt19937 gen(42);

  pasta::RunMode mode = pasta::RunMode::IncrementalPartition;

  pasta::Graph graph("../benchmarks/manual.txt", mode, 1);

  std::cout << "Original Taskflow topology\n";
  graph.dump_graph();

  std::cout << "num_nodes: " << graph.num_nodes() << "\n";
  std::cout << "num_edges: " << graph.num_edges() << "\n";

  graph.run_graph_cudaflow_partition_update(2);

}




















