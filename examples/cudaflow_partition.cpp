#include "pasta.hpp"

int main() {

  pasta::Graph partitioner("../benchmarks/des_perf.txt"); 

  // each task in the task graph is a NxN square matrix multiplication
  size_t matrix_size = 8;  

  if(!partitioner.has_cycle_before_partition()) {
    partitioner.run_graph_before_partition(8);
  }
  else {
    std::cerr << "input graph has cycle.\n";
    std::exit(EXIT_FAILURE);
  }

  partitioner.run_graph_cudaflow_partition(matrix_size, 8);
  
  if(!partitioner.is_cudaflow_partition_share_same_topo_order()) {
    std::cerr << "cudaflow partitioned graph has cycle.\n";
    std::exit(EXIT_FAILURE);
  }

  return 0;
}
