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
    std::cerr << "usage: ./example/modify_edges_with_taskflow matrix_size num_incre_ops circuit_file\n";
    std::exit(EXIT_FAILURE);
  }

  int matrix_size = std::atoi(argv[1]);
  int num_incre_ops = std::atoi(argv[2]);
  std::string circuit_file = argv[3];

  pasta::RunMode mode = pasta::RunMode::IncrementalPartition;

  pasta::Graph graph(circuit_file, mode, matrix_size); 

  int max_parallelism = 8;

  std::cout << "benchmark: " << circuit_file << "\n";
  std::cout << "num_nodes: " << graph.num_nodes() << "\n";
  std::cout << "num_edges: " << graph.num_edges() << "\n";

  size_t N = num_incre_ops;

  size_t num_incre_itr = 1000; // we will have totally 1k incremental iterations

  size_t count = 0;

  std::mt19937 gen(42);

  int num_streams = max_parallelism; // start at 8
  int dir = -1;                          // going down first: 8->7->...->1

  while (count < num_incre_itr) {

    // std::cout << "|------------ running " << count + 1 << " th iteration. -------------------|\n";

    // run with current setting
    graph.run_graph_incre_partition(matrix_size, num_streams, max_parallelism);

    // get N random numbers
    std::vector<int> random_edges = generate_random_nums(graph.num_edges(), N, gen);
    std::sort(random_edges.begin(), random_edges.end());

    // remove N edges randomly
    graph.remove_random_edges(N, gen, mode);

    // add N edges randomly
    graph.add_random_edges(N, gen, 20, mode); 

    if(graph.has_cycle_before_partition() == true) {
      std::cerr << "has cycle!\n";
      std::exit(EXIT_FAILURE);
    }

    // update semaphore for next iteration: bounce between [1, max_parallelism]
    num_streams += dir;
    if (num_streams <= 1) {
      num_streams = 1;
      dir = +1;
    } else if (num_streams >= max_parallelism) {
      num_streams = max_parallelism;
      dir = -1;
    }

    ++count;
  }

  std::cout << "avg critical path length (original): " 
            << static_cast<double>(graph.get_critical_path_length_original()) / num_incre_itr << "\n";
  std::cout << "avg critical path length (constrained): " 
            << static_cast<double>(graph.get_critical_path_length_constrained()) / num_incre_itr << "\n";
  std::cout << "taskflow runtime: " << graph.get_incre_pasta_taskflow_runtime() << "us\n"; 

  return 0;
}
























