#ifndef TIGER_LIVENESS_FLOWGRAPH_H_
#define TIGER_LIVENESS_FLOWGRAPH_H_

#include "tiger/codegen/assem.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/util/graph.h"

namespace fg {

using FNode = graph::Node<assem::Instr>;
using FNodePtr = graph::Node<assem::Instr> *;
using FNodeListPtr = graph::NodeList<assem::Instr> *;
using FGraph = graph::Graph<assem::Instr>;
using FGraphPtr = graph::Graph<assem::Instr> *;

// this is not strictly a factory class
class FlowGraphFactory {
public:
  explicit FlowGraphFactory(assem::InstrList *instr_list)
      : instr_list_(instr_list), flowgraph_(new FGraph()),
        label_map_(std::make_unique<tab::Table<temp::Label, FNode>>()),
        instr_map_(std::make_unique<tab::Table<assem::Instr, FNode>>()) {
    AssemFlowGraph();
  }
  FGraphPtr GetFlowGraph() { return flowgraph_; }

private:
  void AssemFlowGraph();
  assem::InstrList *instr_list_;
  FGraphPtr flowgraph_;
  std::unique_ptr<tab::Table<temp::Label, FNode>> label_map_;
  std::unique_ptr<tab::Table<assem::Instr, FNode>> instr_map_;
};

} // namespace fg

#endif