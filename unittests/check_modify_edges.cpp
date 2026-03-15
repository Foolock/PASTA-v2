#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest.h>
#include "pasta.hpp"

#include <algorithm>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

// --------------------------------------------------------
// Helpers
// --------------------------------------------------------

std::vector<int> generate_random_nums(
  int N,
  int count,
  std::mt19937& gen
) {
  if(count > N) {
    throw std::invalid_argument("count must be <= N");
  }

  std::vector<int> nums(N);
  std::iota(nums.begin(), nums.end(), 0);

  for(int i = 0; i < count; ++i) {
    std::uniform_int_distribution<int> dist(i, N - 1);
    std::swap(nums[i], nums[dist(gen)]);
  }

  nums.resize(count);
  return nums;
}

// --------------------------------------------------------
// Configurable randomized incremental test
// --------------------------------------------------------

static void run_incremental_topology_test(
  const std::string& bench_name,
  int num_incre_ops,
  int num_iters,
  unsigned seed = 42
) {
  pasta::Graph graph(std::string("../../benchmarks/") + bench_name);

  std::mt19937 gen(seed);
  pasta::RunMode mode = pasta::RunMode::Partition;

  // initial check
  REQUIRE(graph.validate_modify_edge() == true);

  for(int iter = 0; iter < num_iters; ++iter) {
    graph.remove_random_edges(num_incre_ops, gen, mode);
    graph.add_random_edges(num_incre_ops, gen, 20, mode);

    REQUIRE(graph.has_cycle_before_partition() == false);

    graph.process_backward_edges();

    REQUIRE(graph.validate_modify_edge() == true);
  }
}

// --------------------------------------------------------
// Macro for benchmark test expansion
// --------------------------------------------------------

#define PASTA_INCREMENTAL_MODIFY_EDGE_TEST(fname, ops, iters)                                     \
  TEST_CASE(("incremental topo maintenance (only modify edges): " fname) * doctest::timeout(300)) {            \
    run_incremental_topology_test(fname, ops, iters);                                      \
  }

// --------------------------------------------------------
// Testcases
// --------------------------------------------------------

PASTA_INCREMENTAL_MODIFY_EDGE_TEST("ac97_ctrl.txt", 8, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("aes_core.txt", 8, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("c1355.txt", 8, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("c17.txt", 4, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("c1908.txt", 8, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("c2670.txt", 8, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("c3540.txt", 8, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("c432.txt", 8, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("c499.txt", 8, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("c5315.txt", 8, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("c6288.txt", 8, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("c7522.txt", 8, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("c880.txt", 8, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("des_perf.txt", 8, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("s1196.txt", 8, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("s1494.txt", 8, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("s27.txt", 4, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("s344.txt", 8, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("s349.txt", 8, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("s400.txt", 8, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("s510.txt", 8, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("simple.txt", 4, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("tv80.txt", 8, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("usb_phy_ispd.txt", 8, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("vga_lcd.txt", 8, 100);
PASTA_INCREMENTAL_MODIFY_EDGE_TEST("wb_dma.txt", 8, 100);

#undef PASTA_INCREMENTAL_MODIFY_EDGE_TEST
