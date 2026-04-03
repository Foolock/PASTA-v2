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
// Configurable randomized incremental test
// --------------------------------------------------------

static void run_incremental_topology_test(
  const std::string& bench_name,
  int num_incre_ops,
  int num_iters,
  unsigned seed = 42
) {

  std::mt19937 gen(seed);
  pasta::RunMode mode = pasta::RunMode::Partition;

  pasta::Graph graph(std::string("../../benchmarks/") + bench_name);

  int max_parallelism = 8;
  int num_streams = max_parallelism; // start at 8
  int dir = -1;                          // going down first: 8->7->...->1

  for(int iter = 0; iter < num_iters; ++iter) {

    REQUIRE(graph.is_cudaflow_satisfy_parallelism(num_streams) == true);

    graph.remove_random_edges(num_incre_ops, gen, mode);
    graph.add_random_edges(num_incre_ops, gen, 20, mode);

    REQUIRE(graph.has_cycle_before_partition() == false);

    // update num_streams for next iteration: bounce between [1, max_parallelism]
    num_streams += dir;
    if (num_streams <= 1) {
      num_streams = 1;
      dir = +1;
    } else if (num_streams >= max_parallelism) {
      num_streams = max_parallelism;
      dir = -1;
    }
  }

}

// --------------------------------------------------------
// Macro for benchmark test expansion
// --------------------------------------------------------

#define CHECK_MODIFY_EDGES_CUDAFLOW(fname, ops, iters)                                     \
  TEST_CASE(("check cudaflow (only modify edges): " fname) * doctest::timeout(300)) {            \
    run_incremental_topology_test(fname, ops, iters);                                      \
  }

// --------------------------------------------------------
// Testcases
// --------------------------------------------------------

CHECK_MODIFY_EDGES_CUDAFLOW("ac97_ctrl.txt", 20, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("aes_core.txt", 20, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("c1355.txt", 20, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("c17.txt", 10, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("c1908.txt", 20, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("c2670.txt", 20, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("c3540.txt", 20, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("c432.txt", 20, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("c499.txt", 20, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("c5315.txt", 20, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("c6288.txt", 20, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("c7522.txt", 20, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("c880.txt", 20, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("des_perf.txt", 20, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("s1196.txt", 20, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("s1494.txt", 20, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("s27.txt", 10, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("s344.txt", 20, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("s349.txt", 20, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("s400.txt", 20, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("s510.txt", 20, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("simple.txt", 5, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("tv80.txt", 20, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("usb_phy_ispd.txt", 20, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("vga_lcd.txt", 20, 100);
CHECK_MODIFY_EDGES_CUDAFLOW("wb_dma.txt", 20, 100);

#undef CHECK_MODIFY_EDGES_CUDAFLOW 
