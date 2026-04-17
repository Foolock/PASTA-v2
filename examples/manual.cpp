#include "pasta.hpp"

int main() {

  std::mt19937 gen(42);

  pasta::RunMode mode = pasta::RunMode::IncrementalPartition;

  pasta::Graph graph("../benchmarks/manual.txt", mode, 1);

  std::cout << "Original Taskflow topology\n";
  graph.dump_graph();

  int matrix_size = 8;
  int max_parallelism = 8;
  std::cout << "num_nodes: " << graph.num_nodes() << "\n";
  std::cout << "num_edges: " << graph.num_edges() << "\n";

  size_t count = 0;

  graph.run_graph_pasta_partition_full(matrix_size, 2, max_parallelism);

}




















