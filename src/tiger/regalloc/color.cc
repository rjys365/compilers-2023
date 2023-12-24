#include "tiger/regalloc/color.h"

#include <limits>

extern frame::RegManager *reg_manager;

namespace col {
/* TODO: Put your lab6 code here */
void Color::init() {
  auto precolored_registers = reg_manager->Registers()->ToSet();
  color_num = precolored_registers.size();
  const auto &node_list = live_graph_->interf_graph->Nodes()->GetList();
  for (const auto &node : node_list) {
    auto *temp = node->NodeInfo();
    temp_node_map_[temp] = node;
    if (precolored_registers.count(temp)) {
      precolored.insert(node);
      // precolored nodes should not be simplified，so set degree to infinity
      degree[node] = 0x7f7f7f7f;
      color_map[node] = node;
    } else {
      initial.insert(node);
      // as we add the edges twice, only need to check one direction
      auto node_degree = node->Succ()->GetList().size();
      degree[node] = node_degree;
    }
  }

  auto &move_list = live_graph_->moves->GetList();
  for (const auto &move : move_list) {
    worklist_moves.insert(move);
    node_move_list[move.first].insert(move);
    node_move_list[move.second].insert(move);
  }
}

void Color::run() {
  make_worklist();
  int round_cnt=0;
  do {
    round_cnt++;
    assert(select_stack.size()==select_stack_set.size());
    if (!simplify_worklist.empty()) {
      simplify();
    } else if (!worklist_moves.empty()) {
      coalesce();
    } else if (!freeze_worklist.empty()) {
      freeze();
    } else if (!spill_worklist.empty()) {
      select_spill();
    }
  } while (!(simplify_worklist.empty() && worklist_moves.empty() &&
             freeze_worklist.empty() && spill_worklist.empty()));
  assign_colors();
}

Result Color::GetResult() {
  Result res;
  if (!spilled_nodes.empty()) {
    for (const auto &node : spilled_nodes) {
      res.spills.push_back(node->NodeInfo());
    }
    return res;
  }
  for(const auto &pair:color_map){
    res.coloring[pair.first->NodeInfo()]=*(reg_manager->temp_map_->Look(pair.second->NodeInfo()));
  }
  return res;
}

void Color::make_worklist() {
  for (const auto &node : initial) {
    // TODO
    if (degree[node] >= color_num) {
      spill_worklist.insert(node);
    } else if (move_related(node)) {
      freeze_worklist.insert(node);
    } else {
      simplify_worklist.insert(node);
    }
  }
  initial.clear();
}

void Color::simplify() {
  auto node = *simplify_worklist.begin();
  simplify_worklist.erase(node);
  select_stack.push(node);
  select_stack_set.insert(node);
  for (const auto &succ : adjacent(node)) {
    decrement_degree(succ);
  }
}

void Color::coalesce() {
  auto move = *worklist_moves.begin();
  worklist_moves.erase(move);
  auto x = get_alias(move.first);
  auto y = get_alias(move.second);
  INodePtr u, v;
  if (is_precolored(y)) {
    u = y;
    v = x;
  } else {
    u = x;
    v = y;
  }
  if (u == v) {
    coalesced_moves.insert(move);
    add_worklist(u);
  } else if (is_precolored(v) || u->Succ()->Contain(v)) {
    constrained_moves.insert(move);
    add_worklist(u);
    add_worklist(v);
  } else if ((is_precolored(u) && george(u, v)) ||
              (!is_precolored(u) && briggs(u, v))) {
    coalesced_moves.insert(move);
    combine(u, v);
    add_worklist(u);
  } else {
    active_moves.insert(move);
  }
}

void Color::freeze() {
  auto node = *freeze_worklist.begin();
  freeze_worklist.erase(node);
  simplify_worklist.insert(node);
  freeze_moves(node);
}

void Color::freeze_moves(INodePtr u) {
  for (const auto &move : node_moves(u)) {
    INodePtr x = move.first;
    INodePtr y = move.second;
    INodePtr v;
    if (get_alias(y) == get_alias(u))
      v = get_alias(x);
    else
      v = get_alias(y);
    active_moves.erase(move);
    frozen_moves.insert(move);
    if (degree[v] < color_num && node_moves(v).empty()) {
      freeze_worklist.erase(v);
      simplify_worklist.insert(v);
    }
  }
}

void Color::select_spill() {
  double min_priority = std::numeric_limits<double>::infinity();
  INodePtr min_node;
  for (const auto &node : spill_worklist) {
    double priority = live_graph_->temp_use_def_cnt[node->NodeInfo()];
    priority /= static_cast<double>(degree[node]);
    if (priority < min_priority) {
      min_priority = priority;
      min_node = node;
    }
  }
  spill_worklist.erase(min_node);
  simplify_worklist.insert(min_node);
  freeze_moves(min_node);
}

void Color::assign_colors() {
  while (!select_stack.empty()) {
    auto node = select_stack.top();
    select_stack.pop();
    select_stack_set.erase(node);
    assert(precolored.count(node)==0);
    auto ok_colors = precolored;
    auto &succ_list = node->Succ()->GetList();
    int contradict_cnt_precolored=0;
    int contradict_cnt_colored=0;
    for (const auto &succ : succ_list) {
      auto succ_alias = get_alias(succ);
      if(colored_nodes.count(succ_alias)){
        ok_colors.erase(color_map[succ_alias]);
        contradict_cnt_colored++;
      }
      if(precolored.count(succ_alias)){
        ok_colors.erase(succ_alias);
        contradict_cnt_precolored++;
      }
      // if (colored_nodes.count(succ_alias) || precolored.count(succ_alias)) {
      //   ok_colors.erase(color_map[succ_alias]);
      // }
    }
    if (ok_colors.empty()) {
      spilled_nodes.insert(node);
    } else {
      colored_nodes.insert(node);
      auto color = *(ok_colors.begin());
      color_map[node] = color;
    }
  }
  for (const auto &node : coalesced_nodes) {
    color_map[node] = color_map[get_alias(node)];
  }
}

std::set<Move> Color::node_moves(INodePtr node) {
  std::set<Move> moves = node_move_list[node];
  std::set<Move> res;
  for (const auto &move : active_moves) {
    if (moves.count(move)) {
      res.insert(move);
    }
  }
  for (const auto &move : worklist_moves) {
    if (moves.count(move)) {
      res.insert(move);
    }
  }
  return res;
}

bool Color::move_related(INodePtr node) { return !node_moves(node).empty(); }

void Color::decrement_degree(INodePtr node) {
  auto d = degree[node];
  degree[node]--;
  if (d == color_num) {
    auto succs = adjacent(node);
    for (const auto &succ : succs) {
      enable_moves(succ);
    }
    enable_moves(node);
    spill_worklist.erase(node);
    if (move_related(node)) {
      freeze_worklist.insert(node);
    } else {
      simplify_worklist.insert(node);
    }
  }
}

void Color::enable_moves(INodePtr node) {
  for (const auto &move : node_moves(node)) {
    if (active_moves.find(move) != active_moves.end()) {
      active_moves.erase(move);
      worklist_moves.insert(move);
    }
  }
}

bool Color::is_precolored(INodePtr node) { return precolored.count(node); }

bool Color::george(INodePtr u, INodePtr v) {
  auto succ_list = adjacent(v);
  for (const auto &t : succ_list) {
    if (!(degree[t] < color_num || is_precolored(t) || t->Succ()->Contain(u))) {
      return false;
    }
  }
  return true;
}

bool Color::briggs(INodePtr u, INodePtr v) {
  std::set<INodePtr> adj;
  auto u_succ_list = adjacent(u);
  for (const auto &t : u_succ_list) {
    if (degree[t] >= color_num) {
      adj.insert(t);
    }
  }
  auto v_succ_list = adjacent(v);
  for (const auto &t : v_succ_list) {
    if (degree[t] >= color_num) {
      adj.insert(t);
    }
  }
  return adj.size() < color_num;
}

std::set<INodePtr> Color::adjacent(INodePtr node) {
  std::set<INodePtr> res;
  auto &succ_list = node->Succ()->GetList();
  for (const auto &succ : succ_list) {
    if (!select_stack_set.count(succ) && !coalesced_nodes.count(succ)) {
      res.insert(succ);
    }
  }
  return res;
}

void Color::add_worklist(INodePtr node) {
  if (!is_precolored(node) && !move_related(node) && degree[node] < color_num) {
    freeze_worklist.erase(node);
    simplify_worklist.insert(node);
  }
}

INodePtr Color::get_alias(INodePtr node) {
  if (coalesced_nodes.count(node)) {
    return get_alias(alias[node]);
  } else {
    return node;
  }
}

void Color::combine(INodePtr u, INodePtr v) {
  if (freeze_worklist.count(v)) {
    freeze_worklist.erase(v);
  } else {
    spill_worklist.erase(v);
  }
  coalesced_nodes.insert(v);
  alias[v] = u;
  node_move_list[u].insert(node_move_list[v].begin(), node_move_list[v].end());
  enable_moves(v);
  auto succ_list = adjacent(v);
  for (const auto &succ : succ_list) {
    if(!succ->Succ()->Contain(u)){
      live_graph_->interf_graph->AddEdge(succ, u);
      live_graph_->interf_graph->AddEdge(u, succ);
      degree[succ]++;
      degree[u]++;
    }
    decrement_degree(succ);
  }
  if (degree[u] >= color_num && freeze_worklist.count(u)) {
    freeze_worklist.erase(u);
    spill_worklist.insert(u);
  }
}

} // namespace col
