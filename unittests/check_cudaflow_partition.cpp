#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest.h>
#include "pasta.hpp"

// --------------------------------------------------------
// Testcase: check partition results of cudaflow partitions 
// --------------------------------------------------------

// Helper macro to avoid repeating the same test body
#define PASTA_CUDAFLOW_PARTITION_TEST(fname)                                             \
  TEST_CASE(("check cudaflow partition results." fname) * doctest::timeout(300)) {       \
    pasta::Graph partitioner(std::string("../../benchmarks/") + fname);                  \
    partitioner.partition_cudaflow(4);                                                   \
    REQUIRE(partitioner.is_cudaflow_partition_share_same_topo_order() == true);          \
  }

// ---- Auto-expanded test cases ----
PASTA_CUDAFLOW_PARTITION_TEST("ac97_ctrl.txt");
PASTA_CUDAFLOW_PARTITION_TEST("aes_core.txt");
PASTA_CUDAFLOW_PARTITION_TEST("c1355.txt");
PASTA_CUDAFLOW_PARTITION_TEST("c17.txt");
PASTA_CUDAFLOW_PARTITION_TEST("c1908.txt");
PASTA_CUDAFLOW_PARTITION_TEST("c2670.txt");
PASTA_CUDAFLOW_PARTITION_TEST("c3540.txt");
PASTA_CUDAFLOW_PARTITION_TEST("c432.txt");
PASTA_CUDAFLOW_PARTITION_TEST("c499.txt");
PASTA_CUDAFLOW_PARTITION_TEST("c5315.txt");
PASTA_CUDAFLOW_PARTITION_TEST("c6288.txt");
PASTA_CUDAFLOW_PARTITION_TEST("c7522.txt");
PASTA_CUDAFLOW_PARTITION_TEST("c880.txt");
PASTA_CUDAFLOW_PARTITION_TEST("des_perf.txt");
PASTA_CUDAFLOW_PARTITION_TEST("s1196.txt");
PASTA_CUDAFLOW_PARTITION_TEST("s1494.txt");
PASTA_CUDAFLOW_PARTITION_TEST("s27.txt");
PASTA_CUDAFLOW_PARTITION_TEST("s344.txt");
PASTA_CUDAFLOW_PARTITION_TEST("s349.txt");
PASTA_CUDAFLOW_PARTITION_TEST("s400.txt");
PASTA_CUDAFLOW_PARTITION_TEST("s510.txt");
PASTA_CUDAFLOW_PARTITION_TEST("simple.txt");
PASTA_CUDAFLOW_PARTITION_TEST("tv80.txt");
PASTA_CUDAFLOW_PARTITION_TEST("usb_phy_ispd.txt");
PASTA_CUDAFLOW_PARTITION_TEST("vga_lcd.txt");
PASTA_CUDAFLOW_PARTITION_TEST("wb_dma.txt");

// Optional: undefine to avoid leaking macro to other files
#undef PASTA_CUDAFLOW_PARTITION_TEST
