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

  size_t rt = 0;
  size_t rt2 = 0;

  graph.run_graph_semaphore(matrix_size, 8);

  std::cout << "total constructtime with semaphore: " << graph.get_incre_runtime_with_semaphore_graph_construct() << " us\n"; 
  std::cout << "total runtime with semaphore: " << graph.get_incre_runtime_with_semaphore() << " us\n"; 
  rt = graph.get_incre_runtime_with_semaphore();
  rt2 = graph.get_incre_runtime_with_semaphore_graph_construct();

  graph.run_graph_semaphore(matrix_size, 6);

  std::cout << "total constructtime with semaphore: " << graph.get_incre_runtime_with_semaphore_graph_construct() - rt2 << " us\n"; 
  std::cout << "total runtime with semaphore: " << graph.get_incre_runtime_with_semaphore() - rt << " us\n"; 
  rt = graph.get_incre_runtime_with_semaphore();
  rt2 = graph.get_incre_runtime_with_semaphore_graph_construct();

  graph.run_graph_semaphore(matrix_size, 4);

  std::cout << "total constructtime with semaphore: " << graph.get_incre_runtime_with_semaphore_graph_construct() - rt2 << " us\n"; 
  std::cout << "total runtime with semaphore: " << graph.get_incre_runtime_with_semaphore() - rt << " us\n"; 
  rt = graph.get_incre_runtime_with_semaphore();
  rt2 = graph.get_incre_runtime_with_semaphore_graph_construct();

  graph.run_graph_semaphore(matrix_size, 2);

  std::cout << "total constructtime with semaphore: " << graph.get_incre_runtime_with_semaphore_graph_construct() - rt2 << " us\n"; 
  std::cout << "total runtime with semaphore: " << graph.get_incre_runtime_with_semaphore() - rt << " us\n"; 
  rt = graph.get_incre_runtime_with_semaphore();
  rt2 = graph.get_incre_runtime_with_semaphore_graph_construct();

  return 0;
}





