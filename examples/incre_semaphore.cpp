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
    std::cerr << "usage: ./example/incre matrix_size num_incre_ops circuit_file\n";
    std::exit(EXIT_FAILURE);
  }

  int matrix_size = std::atoi(argv[1]);
  int num_incre_ops = std::atoi(argv[2]);
  std::string circuit_file = argv[3];

  pasta::Graph graph(circuit_file); 

  int max_parallelism = 8;

  std::cout << "benchmark: " << circuit_file << "\n";
  std::cout << "num_nodes: " << graph.num_nodes() << "\n";
  std::cout << "num_edges: " << graph.num_edges() << "\n";

  size_t N = num_incre_ops;

  size_t num_incre_itr = 100; // we will have totally 1k incremental iterations

  size_t count = 0;

  int num_semaphore = max_parallelism; // start at 8
  int dir = -1;                          // going down first: 8->7->...->1

  std::mt19937 gen(42);
  
  bool run_semaphore = true;
  bool first_run = true;

  while (count < num_incre_itr) {

    // std::cout << "---------------------\n";
    // std::cout << "running " << count + 1 << " th incremental iteration.\n";
    // std::cout << "---------------------\n";

    // run with current semaphore setting
    graph.run_graph_semaphore(matrix_size, num_semaphore, first_run);

    first_run = false;

    // get N random numbers
    std::vector<int> random_nodes = generate_random_nums(graph.num_nodes(), N, gen);
    std::vector<int> random_edges = generate_random_nums(graph.num_edges(), N, gen);
    std::sort(random_nodes.begin(), random_nodes.end());
    std::sort(random_edges.begin(), random_edges.end());

    // remove N nodes randomly
    graph.remove_random_nodes(N, gen, run_semaphore);

    // remove N edges randomly
    graph.remove_random_edges(N, gen, run_semaphore);

    // add N edges randomly
    graph.add_random_edges(N, gen, run_semaphore); 

    // add N nodes randomly by connectint the new nodes 
    // to the existing nodes as dependents/successors  
    graph.add_random_nodes(N, gen, "new", run_semaphore, matrix_size);

    if(graph.has_cycle_before_partition() == true) {
      std::cerr << "has cycle!\n";
      std::exit(EXIT_FAILURE);
    }

    // update semaphore for next iteration: bounce between [1, max_parallelism]
    num_semaphore += dir;
    if (num_semaphore <= 1) {
      num_semaphore = 1;
      dir = +1;
    } else if (num_semaphore >= max_parallelism) {
      num_semaphore = max_parallelism;
      dir = -1;
    }

    ++count;
  }

  std::cout << "total constructtime with semaphore: " << graph.get_incre_runtime_with_semaphore_graph_construct() << " ms\n"; 
  std::cout << "total runtime with semaphore: " << graph.get_incre_runtime_with_semaphore() << " ms\n"; 

  return 0;
}
























