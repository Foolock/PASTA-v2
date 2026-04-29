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

  if(argc != 4) {
    std::cerr << "usage: ./example/modify_nodes_with_semaphore matrix_size num_incre_ops circuit_file\n";
    std::exit(EXIT_FAILURE);
  }

  int matrix_size = std::atoi(argv[1]);
  int num_incre_ops = std::atoi(argv[2]);
  std::string circuit_file = argv[3];

  pasta::RunMode mode = pasta::RunMode::Semaphore;

  pasta::Graph graph(circuit_file, mode, matrix_size);

  int max_parallelism = 8;

  std::cout << "benchmark: " << circuit_file << "\n";
  std::cout << "num_nodes: " << graph.num_nodes() << "\n";
  std::cout << "num_edges: " << graph.num_edges() << "\n";

  size_t N = num_incre_ops;
  size_t num_incre_itr = 1000;

  size_t count = 0;

  std::mt19937 gen(42);

  int num_semaphore = max_parallelism;  // start at 8

  while (count < num_incre_itr) {

    // run with current setting
    graph.run_graph_semaphore(num_semaphore);

    // remove N edges randomly
    graph.remove_random_edges(N, gen, mode);

    // add N edges randomly
    graph.add_random_edges(N, gen, 20, mode);

    // remove N nodes randomly
    graph.remove_random_nodes(N, gen, mode);

    // add N nodes randomly, each connected to one random existing node
    graph.add_random_nodes(N, gen, "new", mode, matrix_size);

    if(graph.has_cycle_before_partition() == true) {
      std::cerr << "has cycle!\n";
      std::exit(EXIT_FAILURE);
    }

    ++count;
  }

  std::cout << "taskflow buildtime (semaphore): " << graph.get_semaphore_taskflow_buildtime() << " us\n";
  std::cout << "taskflow runtime (semaphore): " << graph.get_semaphore_taskflow_runtime() << " us\n";

  return 0;
}
