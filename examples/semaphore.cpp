#include "pasta.hpp"

int main(int argc, char* argv[]) {

  if(argc != 3) {
    std::cerr << "usage: ./example/semaphore matrix_size circuit_file\n";
    std::exit(EXIT_FAILURE);
  }

  int matrix_size = std::atoi(argv[1]);
  std::string circuit_file = argv[2];

  pasta::Graph graph(circuit_file); 

  int max_parallelism = 8;

  std::cout << "benchmark: " << circuit_file << "\n";
  std::cout << "num_nodes: " << graph.num_nodes() << "\n";
  std::cout << "num_edges: " << graph.num_edges() << "\n";

  int num_semaphore = max_parallelism; // start at 8

  graph.run_graph_semaphore(matrix_size, num_semaphore);

  std::cout << "total runtime with semaphore: " << graph.get_incre_runtime_with_semaphore() << " ms\n"; 

  return 0;
}





