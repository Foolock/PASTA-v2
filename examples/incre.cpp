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

  int max_num_semaphore = 8;

  std::cout << "num_nodes: " << graph.num_nodes() << "\n";
  std::cout << "num_edges: " << graph.num_edges() << "\n";

  graph.run_graph_semaphore(matrix_size, max_num_semaphore);

  size_t N = num_incre_ops;

  size_t num_incre_itr = 2; // we will have totally 1k incremental iterations

  size_t count = 0;

  std::mt19937 gen(42);

  while (count < num_incre_itr) {

    std::cout << "---------------------\n";
    std::cout << "running " << count + 1 << " th incremental iteration.\n";
    std::cout << "---------------------\n";

    // get N random numbers
    std::vector<int> random_nodes = generate_random_nums(graph.num_nodes(), N, gen);
    std::vector<int> random_edges = generate_random_nums(graph.num_edges(), N, gen);
    std::sort(random_nodes.begin(), random_nodes.end());
    std::sort(random_edges.begin(), random_edges.end());

    // remove N nodes randomly
    graph.remove_random_nodes(N, gen);

    // remove N edges randomly
    graph.remove_random_edges(N, gen);

    ++count;
  }

  return 0;
}
























