#ifndef TIGER_LIVENESS_LIVENESS_H_
#define TIGER_LIVENESS_LIVENESS_H_

#include <set>

#include "tiger/codegen/assem.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/x64frame.h"
#include "tiger/liveness/flowgraph.h"
#include "tiger/util/graph.h"

namespace live {

using INode = graph::Node<temp::Temp>;
using INodePtr = graph::Node<temp::Temp> *;
using INodeList = graph::NodeList<temp::Temp>;
using INodeListPtr = graph::NodeList<temp::Temp> *;
using IGraph = graph::Graph<temp::Temp>;
using IGraphPtr = graph::Graph<temp::Temp> *;

class MoveList {
public:
  MoveList() = default;

  [[nodiscard]] const std::list<std::pair<INodePtr, INodePtr>> &
  GetList() const {
    return move_list_;
  }
  void Append(INodePtr src, INodePtr dst) { move_list_.emplace_back(src, dst); }
  bool Contain(INodePtr src, INodePtr dst);
  void Delete(INodePtr src, INodePtr dst);
  void Prepend(INodePtr src, INodePtr dst) {
    move_list_.emplace_front(src, dst);
  }
  MoveList *Union(MoveList *list);
  MoveList *Intersect(MoveList *list);

private:
  std::list<std::pair<INodePtr, INodePtr>> move_list_;
};

struct LiveGraph {
  IGraphPtr interf_graph;
  MoveList *moves;
  std::unordered_map<temp::Temp *, int> temp_use_def_cnt;

  LiveGraph(IGraphPtr interf_graph, MoveList *moves)
      : interf_graph(interf_graph), moves(moves) {}
};

class LiveGraphFactory {
public:
  explicit LiveGraphFactory(fg::FGraphPtr flowgraph)
      : flowgraph_(flowgraph), live_graph_(new IGraph(), new MoveList()) {
        Liveness();
      }
  // in_(std::make_unique<graph::Table<assem::Instr, temp::TempList>>()),
  // out_(std::make_unique<graph::Table<assem::Instr, temp::TempList>>()),
  // temp_node_map_(new tab::Table<temp::Temp, INode>())

  void Liveness();
  LiveGraph GetLiveGraph() { return live_graph_; }
  const std::unordered_map<temp::Temp *, INodePtr> &GetTempNodeMap() {
    return temp_node_map_;
  }
  const std::unordered_map<temp::Temp *, int> &GetTempUseDefCnt() {
    return live_graph_.temp_use_def_cnt;
  }

private:
  fg::FGraphPtr flowgraph_;
  LiveGraph live_graph_;

  // std::unique_ptr<graph::Table<assem::Instr, temp::TempList>> in_;
  // std::unique_ptr<graph::Table<assem::Instr, temp::TempList>> out_;
  // use std set instead of temp list
  std::unordered_map<assem::Instr *, std::set<temp::Temp *>> in_;
  std::unordered_map<assem::Instr *, std::set<temp::Temp *>> out_;
  // tab::Table<temp::Temp, INode> *temp_node_map_;
  std::unordered_map<temp::Temp *, INodePtr> temp_node_map_;

  void LiveMap();
  void InterfGraph();
};

} // namespace live

#endif