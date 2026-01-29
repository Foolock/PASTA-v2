#include "pasta.hpp"

int main() {

  pasta::Graph graph;

  pasta::Node* A = graph.insert_node("A");
  pasta::Node* B = graph.insert_node("B");
  pasta::Node* C = graph.insert_node("C");
  pasta::Node* D = graph.insert_node("D");

  pasta::Edge* AC = graph.insert_edge(A, C);
  pasta::Edge* AD = graph.insert_edge(A, D);
  pasta::Edge* BD = graph.insert_edge(B, D);

  graph.dump_graph();

  graph.insert_edge(B, C);
  graph.dump_graph();

  graph.remove_edge(AC);
  graph.dump_graph();

}
