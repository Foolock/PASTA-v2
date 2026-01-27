#pragma once

#include <iostream>
#include <string>
#include "taskflow/taskflow.hpp"
#include "wsq.hpp"

namespace pasta {

class Node;
class Edge;
class CNode;
class CEdge;
class Graph;

class Node {

  friend class Graph;

  public:
    Node(const std::string& name) : _name(name) {};

    inline size_t num_fanins() const {
      return _fanins.size();
    }

    inline size_t num_fanouts() const {
      return _fanouts.size();
    }

    inline std::string name() {
      return _name;
    }

  private:
    std::string _name;  
    std::list<Edge*> _fanins;
    std::list<Edge*> _fanouts;
    bool _visistead = false;
    tf::Task _task;
    int _cluster_id = -1; // specify which partition (cluster) it belongs
    std::atomic<size_t> _dep_cnt{0};
    CNode* _cnode = NULL; // specify which cnode (cluster) it belongs

};

class Edge {

  friend class Graph;

  private:
    Node* _from;
    Node* _to;

};

class Graph {

  public:
    // constructors
    Graph() {};
    Graph(const std::string& filename);

};

} // end of namespace pasta


























