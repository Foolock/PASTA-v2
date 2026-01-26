#pragma once

#include <iostream>
#include <string>

namespace pasta {

class Node;
class Edge;
class Graph;

class Graph {

  public:
    // constructors
    Graph() {};
    Graph(const std::string& filename);

};

} // end of namespace pasta
