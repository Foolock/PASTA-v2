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

  if(argc != 3) {
    std::cerr << "usage: ./example/modify_edges num_incre_ops circuit_file\n";
    std::exit(EXIT_FAILURE);
  }

  int num_incre_ops = std::atoi(argv[1]);
  std::string circuit_file = argv[2];

  pasta::Graph graph(circuit_file); 

  int max_parallelism = 8;

  std::cout << "benchmark: " << circuit_file << "\n";
  std::cout << "num_nodes: " << graph.num_nodes() << "\n";
  std::cout << "num_edges: " << graph.num_edges() << "\n";

  size_t N = num_incre_ops;

  size_t num_incre_itr = 100; // we will have totally 1k incremental iterations

  size_t count = 0;

  std::mt19937 gen(42);

  pasta::RunMode mode = pasta::RunMode::Partition;

  graph.test_func();

  while (count < num_incre_itr) {

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

    graph.process_backward_edges();

    graph.generate_topo_order();

    if(!graph.validate_modify_edge()) {
      std::cerr << "num_itr = " << num_incre_itr << "\n";
      throw std::runtime_error("The topological order is not maintained correctly");
    }

    ++count;
  }

  std::cout << "time spent on incrementally maintaining topo order: " << graph.get_process_backward_edge_time() << "us\n"; 
  std::cout << "time spent on regenerating topo order: " << graph.get_generate_topo_order_time() << "us\n"; 

  return 0;
}
























