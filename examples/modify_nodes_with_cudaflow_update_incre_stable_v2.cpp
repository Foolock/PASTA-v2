#include "pasta.hpp"

std::vector<int> generate_random_nums(
    int N,
    int count,
    std::mt19937& gen
) {
    if (count > N) {
        throw std::invalid_argument("count must be <= N");
    }

    std::vector<int> nums(N);
    std::iota(nums.begin(), nums.end(), 0);

    for (int i = 0; i < count; ++i) {
        std::uniform_int_distribution<int> dist(i, N - 1);
        std::swap(nums[i], nums[dist(gen)]);
    }

    nums.resize(count);
    return nums;
}


int main(int argc, char* argv[]) {

  if(argc != 4 && argc != 5) {
    std::cerr << "usage: ./example/modify_nodes_with_cudaflow_update_incre_stable_v2 "
                 "matrix_size num_incre_ops circuit_file [num_streams]\n";
    std::cerr << "  num_streams: parallelism limit held constant across iterations "
                 "(default 8)\n";
    std::exit(EXIT_FAILURE);
  }

  int matrix_size = std::atoi(argv[1]);
  int num_incre_ops = std::atoi(argv[2]);
  std::string circuit_file = argv[3];
  int num_streams = (argc == 5) ? std::atoi(argv[4]) : 8;

  if(num_streams < 1) {
    std::cerr << "num_streams must be >= 1\n";
    std::exit(EXIT_FAILURE);
  }

  pasta::RunMode mode = pasta::RunMode::IncrementalPartition;

  pasta::Graph graph(circuit_file, mode, matrix_size);

  std::cout << "benchmark: " << circuit_file << "\n";
  std::cout << "num_nodes: " << graph.num_nodes() << "\n";
  std::cout << "num_edges: " << graph.num_edges() << "\n";
  std::cout << "num_streams (stable, v2): " << num_streams << "\n";

  size_t N = num_incre_ops;
  size_t num_incre_itr = 1000;

  size_t count = 0;

  std::mt19937 gen(42);

  while (count < num_incre_itr) {

    // v2: persistent stream edges across iterations, reconciled via diff in Step 3.
    graph.run_graph_cudaflow_partition_update_incre_v2(num_streams);

    graph.remove_random_edges(N, gen, mode);
    graph.add_random_edges(N, gen, 20, mode);
    graph.remove_random_nodes(N, gen, mode);
    graph.add_random_nodes(N, gen, "new", mode, matrix_size);

    if(graph.has_cycle_before_partition() == true) {
      std::cerr << "has cycle!\n";
      std::exit(EXIT_FAILURE);
    }

    ++count;
  }

  std::cout << "avg critical path length (cudaflow v2): "
            << static_cast<double>(graph.get_critical_path_length_constrained()) / num_incre_itr << "\n";
  std::cout << "incremental level list runtime (cudaflow v2): " << graph.get_cudaflow_incre_level_list_runtime() << " us\n";
  std::cout << "assign streams runtime (cudaflow v2): " << graph.get_cudaflow_assign_streams_runtime() << " us\n";
  std::cout << "taskflow buildtime (cudaflow v2): " << graph.get_cudaflow_taskflow_buildtime() << " us\n";
  std::cout << "taskflow runtime (cudaflow v2): " << graph.get_incre_runtime_with_cudaflow_partition() << " us\n";

  return 0;
}
