#include "pasta.hpp"

int main() {

  pasta::Graph graph;

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

  pasta::Node* n1 = graph.insert_node("n1");
  pasta::Node* n2 = graph.insert_node("n2");
  pasta::Node* n3 = graph.insert_node("n3");
  pasta::Node* n4 = graph.insert_node("n4");
  pasta::Node* n5 = graph.insert_node("n5");
  pasta::Node* n6 = graph.insert_node("n6");
  pasta::Node* n7 = graph.insert_node("n7");

  pasta::Edge* n1n3 = graph.insert_edge(n1, n3);
  pasta::Edge* n1n4 = graph.insert_edge(n1, n4);
  pasta::Edge* n1n5 = graph.insert_edge(n1, n5);
  pasta::Edge* n3n7 = graph.insert_edge(n3, n7);
  pasta::Edge* n4n7 = graph.insert_edge(n4, n7);
  pasta::Edge* n5n7 = graph.insert_edge(n5, n7);
  pasta::Edge* n3n6 = graph.insert_edge(n3, n6);

  graph.dump_graph();

  graph.test_func();

}




















