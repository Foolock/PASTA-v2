#include "pasta.hpp"

int main() {

  std::mt19937 gen(42);

  pasta::RunMode mode = pasta::RunMode::IncrementalPartition;

  pasta::Graph graph("../benchmarks/manual.txt", mode, 1);

  std::cout << "Original Taskflow topology\n";
  graph.dump_graph();

  // pasta::Node* A = graph.insert_node("A");
  // pasta::Node* B = graph.insert_node("B");
  // pasta::Node* C = graph.insert_node("C");
  // pasta::Node* D = graph.insert_node("D");

  // pasta::Edge* AC = graph.insert_edge(A, C);
  // pasta::Edge* AD = graph.insert_edge(A, D);
  // pasta::Edge* BD = graph.insert_edge(B, D);

  // graph.dump_graph();

  // graph.insert_edge(B, C);
  // graph.dump_graph();

  // graph.remove_edge(AC);
  // graph.dump_graph();

  // pasta::Node* n0 = graph.insert_node("0");
  // pasta::Node* n1 = graph.insert_node("1");
  // pasta::Node* n2 = graph.insert_node("2");
  // pasta::Node* n3 = graph.insert_node("3");
  // pasta::Node* n4 = graph.insert_node("4");
  // pasta::Node* n5 = graph.insert_node("5");
  // pasta::Node* n6 = graph.insert_node("6");
  // pasta::Node* n7 = graph.insert_node("7");
  // pasta::Node* n8 = graph.insert_node("8");
  // pasta::Node* n9 = graph.insert_node("9");
  // pasta::Node* n10 = graph.insert_node("10");

  // graph.insert_edge(n0, n1);
  // graph.insert_edge(n1, n2);
  // graph.insert_edge(n2, n3);
  // graph.insert_edge(n3, n10);
  // graph.insert_edge(n0, n4);
  // graph.insert_edge(n4, n5);
  // graph.insert_edge(n5, n6);
  // graph.insert_edge(n6, n10);
  // graph.insert_edge(n0, n7);
  // graph.insert_edge(n7, n8);
  // graph.insert_edge(n8, n9);
  // graph.insert_edge(n9, n10);

  int matrix_size = 8;
  int num_incre_ops = 10;
  int max_parallelism = 8;

  // graph.remove_random_edges(10, gen, mode);
  // // graph.remove_actual_edge();

  // std::cout << "After removing edge\n";
  // graph.dump_graph();

  // if(!graph.is_taskflow_topo_consistent()) {
  //   std::cerr << "Graph not consistent\n";
  //   std::exit(EXIT_FAILURE);
  // }

  std::cout << "num_nodes: " << graph.num_nodes() << "\n";
  std::cout << "num_edges: " << graph.num_edges() << "\n";

  size_t num_incre_itr = 100; // we will have totally 1k incremental iterations

  size_t count = 0;

  // graph.remove_random_edges(5, gen, mode);

  graph.run_graph_incre_partition(matrix_size, 2, max_parallelism);

  graph.add_backward_edge(); 
  std::cout << "Original graph after adding edges\n";
  graph.dump_graph();

  graph.run_graph_incre_partition(matrix_size, 2, max_parallelism);
}




















