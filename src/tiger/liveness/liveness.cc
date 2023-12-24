#include "tiger/liveness/liveness.h"

extern frame::RegManager *reg_manager;

namespace live {

bool MoveList::Contain(INodePtr src, INodePtr dst) {
  return std::any_of(move_list_.cbegin(), move_list_.cend(),
                     [src, dst](std::pair<INodePtr, INodePtr> move) {
                       return move.first == src && move.second == dst;
                     });
}

void MoveList::Delete(INodePtr src, INodePtr dst) {
  assert(src && dst);
  auto move_it = move_list_.begin();
  for (; move_it != move_list_.end(); move_it++) {
    if (move_it->first == src && move_it->second == dst) {
      break;
    }
  }
  move_list_.erase(move_it);
}

MoveList *MoveList::Union(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : move_list_) {
    res->move_list_.push_back(move);
  }
  for (auto move : list->GetList()) {
    if (!res->Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::Intersect(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : list->GetList()) {
    if (Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

void LiveGraphFactory::LiveMap() {
  /* TODO: Put your lab6 code here */
  const auto &flowgraph_node_list = flowgraph_->Nodes()->GetList();
  for (const auto &node : flowgraph_node_list) {
    auto *instr = node->NodeInfo();
    in_[instr] = std::set<temp::Temp *>();
    out_[instr] = std::set<temp::Temp *>();
  }
  bool done=true;
  do{
    done=true;
    for (const auto &node : flowgraph_node_list) {
      auto *instr = node->NodeInfo();
      auto &in_set = in_[instr];
      auto &out_set = out_[instr];
      auto def_set = instr->Def();
      auto use_set = instr->Use();
      std::set<temp::Temp *> new_in_set=out_set;
      for(const auto &d:def_set){
        new_in_set.erase(d);
      }
      // new_in_set.erase(def_set.begin(), def_set.end());
      new_in_set.insert(use_set.begin(), use_set.end());
      if(new_in_set!=in_set){
        done=false;
        in_set=new_in_set;
      }
      auto succs_list = node->Succ()->GetList();
      std::set<temp::Temp *> new_out_set;
      for(const auto &succ:succs_list){
        auto *succ_instr=succ->NodeInfo();
        auto &succ_in_set=in_[succ_instr];
        new_out_set.insert(succ_in_set.begin(), succ_in_set.end());
      }
      if(new_out_set!=out_set){
        done=false;
        out_set=new_out_set;
      }
    } 
  }while(!done);
  // calculate temp_use_def_cnt for heuristic spilling
  for (const auto &node : flowgraph_node_list) {
    auto *instr = node->NodeInfo();
    auto def_set = instr->Def();
    auto use_set = instr->Use();
    for(const auto &d:def_set){
      live_graph_.temp_use_def_cnt[d]++;
    }
    for(const auto &u:use_set){
      live_graph_.temp_use_def_cnt[u]++;
    }
  }
}

void LiveGraphFactory::InterfGraph() { 
  /* TODO: Put your lab6 code here */ 
  const auto &registers_list=reg_manager->Registers()->GetList();
  for(const auto &reg:registers_list){
    auto *reg_node=live_graph_.interf_graph->NewNode(reg);
    temp_node_map_[reg]=reg_node;
  }
  for(const auto &reg:registers_list){
    for(const auto &reg2:registers_list){
      if(reg==reg2)
        continue;
      live_graph_.interf_graph->AddEdge(temp_node_map_[reg], temp_node_map_[reg2]);
    }
  }
  const auto &flowgraph_node_list = flowgraph_->Nodes()->GetList();
  for (const auto &node: flowgraph_node_list){
    auto *instr=node->NodeInfo();
    auto def_set = instr->Def();
    auto use_set = instr->Use();
    for(const auto &d:def_set){
      if(temp_node_map_.find(d)==temp_node_map_.end()){
        auto *d_node=live_graph_.interf_graph->NewNode(d);
        temp_node_map_[d]=d_node;
      }
    }
    for(const auto &u:use_set){
      if(temp_node_map_.find(u)==temp_node_map_.end()){
        auto *u_node=live_graph_.interf_graph->NewNode(u);
        temp_node_map_[u]=u_node;
      }
    }
  }
  for (const auto &node : flowgraph_node_list) {
    auto *instr = node->NodeInfo();
    auto def_set = instr->Def();
    
    auto live_out_set = out_[instr];
    if(auto *move_instr=dynamic_cast<assem::MoveInstr *>(instr)){
      auto use_set = instr->Use();
      assert(use_set.size()==1&&def_set.size()==1);
      auto *src_temp=*(use_set.begin());
      auto *dst_temp=*(def_set.begin());
      if(src_temp==dst_temp)continue;
      live_out_set.erase(src_temp);
      if(!live_graph_.moves->Contain(temp_node_map_[src_temp],temp_node_map_[dst_temp])){
        live_graph_.moves->Append(temp_node_map_[src_temp],temp_node_map_[dst_temp]);
      }
    }
    live_out_set.insert(def_set.begin(), def_set.end());
    for(const auto &d:def_set){
      for(const auto &l:live_out_set){
        if(d==l)continue;
        live_graph_.interf_graph->AddEdge(temp_node_map_[d], temp_node_map_[l]);
        live_graph_.interf_graph->AddEdge(temp_node_map_[l], temp_node_map_[d]);
      }
    }
  }
}

void LiveGraphFactory::Liveness() {
  LiveMap();
  InterfGraph();
}

} // namespace live
