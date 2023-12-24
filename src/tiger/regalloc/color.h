#ifndef TIGER_COMPILER_COLOR_H
#define TIGER_COMPILER_COLOR_H

#include "tiger/codegen/assem.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/util/graph.h"

#include <stack>

extern frame::RegManager *reg_manager;

namespace col {
using live::INodePtr;
using Move=std::pair<INodePtr, INodePtr>;
struct Result {
  std::unordered_map<temp::Temp *, std::string> coloring;
  std::list<temp::Temp *> spills;
};

class Color {
  /* TODO: Put your lab6 code here */

public:
  Color(live::LiveGraph *live_graph) : live_graph_(live_graph) {
    init();
    run();
  }
  Result GetResult();

private:
  live::LiveGraph *live_graph_;
  std::unordered_map<temp::Temp *, INodePtr> temp_node_map_;

  std::set<INodePtr> precolored;
  std::set<INodePtr> initial;
  std::set<INodePtr> simplify_worklist;
  std::set<INodePtr> freeze_worklist;
  std::set<INodePtr> spill_worklist;
  std::set<INodePtr> spilled_nodes;
  std::set<INodePtr> coalesced_nodes;
  std::set<INodePtr> colored_nodes;
  std::stack<INodePtr> select_stack;
  std::set<INodePtr> select_stack_set;

  std::set<Move> coalesced_moves;
  std::set<Move> constrained_moves;
  std::set<Move> frozen_moves;
  std::set<Move> worklist_moves;
  std::set<Move> active_moves;

  std::unordered_map<INodePtr, int> degree;
  std::unordered_map<INodePtr, INodePtr> alias;
  std::unordered_map<INodePtr, INodePtr> color_map;
  std::unordered_map<INodePtr, std::set<Move>>
      node_move_list;

  int color_num;

  void init();
  void make_worklist();
  void simplify();
  void coalesce();
  void freeze();
  void select_spill();
  void assign_colors();
  void run();

  std::set<INodePtr> adjacent(INodePtr node);
  std::set<Move> node_moves(INodePtr node);
  bool move_related(INodePtr node);
  void decrement_degree(INodePtr node);
  void enable_moves(INodePtr node);
  void add_worklist(INodePtr node);
  INodePtr get_alias(INodePtr node);
  bool is_precolored(INodePtr node);
  bool george(INodePtr u, INodePtr v);
  bool briggs(INodePtr u, INodePtr v);
  void combine(INodePtr u, INodePtr v);
  void freeze_moves(INodePtr node);

};
} // namespace col

#endif // TIGER_COMPILER_COLOR_H
