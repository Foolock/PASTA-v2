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
// Configurable randomized incremental test for v2:
//   - run_graph_cudaflow_partition_update_incre_v2 maintains stream edges
//     persistently across iterations.
//   - verify_cudaflow_partition_update_incre_v2 expects v2 state to exist,
//     so we call run_*_v2 inside the loop before mutations and verify after.
// --------------------------------------------------------

static void run_incremental_node_test_v2(
  const std::string& bench_name,
  int num_incre_ops,
  int num_iters,
  int max_parallelism,
  unsigned seed = 42
) {

  std::mt19937 gen(seed);
  pasta::RunMode mode = pasta::RunMode::IncrementalPartition;

  pasta::Graph graph(std::string("../../benchmarks/") + bench_name, mode, 1);

  // initial check (v2 partition not installed yet)
  REQUIRE(graph.has_cycle_before_partition() == false);

  for(int iter = 0; iter < num_iters; ++iter) {
    // mutations FIRST
    graph.remove_random_edges(num_incre_ops, gen, mode);
    graph.add_random_edges(num_incre_ops, gen, 20, mode);
    graph.remove_random_nodes(num_incre_ops, gen, mode);
    graph.add_random_nodes(num_incre_ops, gen, "new", mode, 1);

    REQUIRE(graph.has_cycle_before_partition() == false);

    // THEN reconcile + run
    graph.run_graph_cudaflow_partition_update_incre_v2(max_parallelism);

    // verify AFTER reconcile, when Taskflow is in execution-ready state
    REQUIRE(graph.verify_cudaflow_partition_update_incre_v2(max_parallelism) == true);
  }
}

#define CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2(fname, ops, iters, maxp)                            \
  TEST_CASE(("check cudaflow partition update incre v2 (modify nodes and edges, persistent stream edges): " fname) \
            * doctest::timeout(300)) {                                                                            \
    run_incremental_node_test_v2(fname, ops, iters, maxp);                                                        \
  }

CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("ac97_ctrl.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("aes_core.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("c1355.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("c17.txt", 10, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("c1908.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("c2670.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("c3540.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("c432.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("c499.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("c5315.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("c6288.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("c7522.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("c880.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("des_perf.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("s1196.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("s1494.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("s27.txt", 10, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("s344.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("s349.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("s400.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("s510.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("simple.txt", 5, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("tv80.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("usb_phy_ispd.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("vga_lcd.txt", 20, 100, 8);
CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2("wb_dma.txt", 20, 100, 8);

#undef CUDAFLOW_CHECK_PARTITION_UPDATE_INCRE_MODIFY_NODES_V2
