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
// Configurable randomized incremental test with node mutations
// --------------------------------------------------------

static void run_incremental_node_test(
  const std::string& bench_name,
  int num_incre_ops,
  int num_iters,
  int max_parallelism,
  unsigned seed = 42
) {

  std::mt19937 gen(seed);
  pasta::RunMode mode = pasta::RunMode::IncrementalPartition;

  pasta::Graph graph(std::string("../../benchmarks/") + bench_name, mode, 1);

  // initial check
  REQUIRE(graph.has_cycle_before_partition() == false);
  REQUIRE(graph.verify_cudaflow_partition_update_incre(max_parallelism) == true);

  for(int iter = 0; iter < num_iters; ++iter) {

    graph.remove_random_edges(num_incre_ops, gen, mode);
    graph.add_random_edges(num_incre_ops, gen, 20, mode);
    graph.remove_random_nodes(num_incre_ops, gen, mode);
    graph.add_random_nodes(num_incre_ops, gen, "new", mode, 1);

    REQUIRE(graph.has_cycle_before_partition() == false);
    REQUIRE(graph.verify_cudaflow_partition_update_incre(max_parallelism) == true);
  }
}

// --------------------------------------------------------
// Macro for benchmark test expansion
// --------------------------------------------------------

#define CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES(fname, ops, iters, maxp)                               \
  TEST_CASE(("check cudaflow partition update incre (modify nodes and edges): " fname) * doctest::timeout(300)) { \
    run_incremental_node_test(fname, ops, iters, maxp);                                                           \
  }

// --------------------------------------------------------
// Testcases
// --------------------------------------------------------

CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("ac97_ctrl.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("aes_core.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("c1355.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("c17.txt", 10, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("c1908.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("c2670.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("c3540.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("c432.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("c499.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("c5315.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("c6288.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("c7522.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("c880.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("des_perf.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("s1196.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("s1494.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("s27.txt", 10, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("s344.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("s349.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("s400.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("s510.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("simple.txt", 5, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("tv80.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("usb_phy_ispd.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("vga_lcd.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES("wb_dma.txt", 20, 100, 8);

#undef CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES
