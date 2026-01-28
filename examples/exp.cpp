#include "pasta.hpp"

int main(int argc, char* argv[]) {

  if(argc != 4) {
    std::cerr << "usage: ./example/exp matrix_size num_semaphore circuit_file\n";
    std::exit(EXIT_FAILURE);
  }

  int matrix_size = std::atoi(argv[1]);
  int num_semaphore = std::atoi(argv[2]);
  std::string circuit_file = argv[3];

  pasta::Graph graph(circuit_file); 

  std::cout << "num_nodes: " << graph.num_nodes() << "\n";
  std::cout << "num_edges: " << graph.num_edges() << "\n";

  graph.run_graph_semaphore(matrix_size, num_semaphore);

  return 0;
}
