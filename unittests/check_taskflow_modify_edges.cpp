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
  pasta::RunMode mode = pasta::RunMode::IncrementalPartition;

  pasta::Graph graph(std::string("../../benchmarks/") + bench_name, mode, 1);

  // initial check
  REQUIRE(graph.validate_modify_edge() == true);

  size_t count = 0;

  for(int iter = 0; iter < num_iters; ++iter) {

    std::cout << "------------------- #iter " << ++count << "-----------------\n"; 

    graph.remove_random_edges(num_incre_ops, gen, mode);
    graph.add_random_edges(num_incre_ops, gen, 20, mode);

    REQUIRE(graph.has_cycle_before_partition() == false);

    REQUIRE(graph.is_taskflow_topo_consistent() == true);
  }
}

// --------------------------------------------------------
// Macro for benchmark test expansion
// --------------------------------------------------------

#define PASTA_CHECK_TASKFLOW_MODIFY_EDGES(fname, ops, iters)                                     \
  TEST_CASE(("check taskflow (only modify edges): " fname) * doctest::timeout(300)) {            \
    run_incremental_topology_test(fname, ops, iters);                                      \
  }

// --------------------------------------------------------
// Testcases
// --------------------------------------------------------

PASTA_CHECK_TASKFLOW_MODIFY_EDGES("ac97_ctrl.txt", 20, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("aes_core.txt", 20, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("c1355.txt", 20, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("c17.txt", 10, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("c1908.txt", 20, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("c2670.txt", 20, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("c3540.txt", 20, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("c432.txt", 20, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("c499.txt", 20, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("c5315.txt", 20, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("c6288.txt", 20, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("c7522.txt", 20, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("c880.txt", 20, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("des_perf.txt", 20, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("s1196.txt", 20, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("s1494.txt", 20, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("s27.txt", 10, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("s344.txt", 20, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("s349.txt", 20, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("s400.txt", 20, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("s510.txt", 20, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("simple.txt", 5, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("tv80.txt", 20, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("usb_phy_ispd.txt", 20, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("vga_lcd.txt", 20, 100);
PASTA_CHECK_TASKFLOW_MODIFY_EDGES("wb_dma.txt", 20, 100);

#undef PASTA_CHECK_TASKFLOW_MODIFY_EDGES
