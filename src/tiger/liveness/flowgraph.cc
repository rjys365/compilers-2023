#include "tiger/liveness/flowgraph.h"

extern frame::RegManager *reg_manager;

namespace fg {

bool is_unconditional_jump(assem::Instr *instr) {
  if (auto oper_instr = dynamic_cast<assem::OperInstr *>(instr)) {
    return oper_instr->assem_.substr(0, 3) == "jmp";
  }
  return false;
}

void FlowGraphFactory::AssemFlowGraph() {
  /* TODO: Put your lab6 code here */
  const auto &instr_list = instr_list_->GetList();
  FNode *prev = nullptr;
  for (auto instr : instr_list) {
    auto instr_node = flowgraph_->NewNode(instr);
    if (auto label_instr = dynamic_cast<assem::LabelInstr *>(instr)) {
      label_map_->Enter(label_instr->label_, instr_node);
    }
    instr_map_->Enter(instr, instr_node);
    if (prev != nullptr && !is_unconditional_jump(prev->NodeInfo())) {
      flowgraph_->AddEdge(prev, instr_node);
    }
    prev = instr_node;
  }

  for (auto instr_it = instr_list.cbegin(); instr_it != instr_list.cend();
       instr_it++) {
    auto instr = *instr_it;
    bool is_jump = false;
    if (auto oper_instr = dynamic_cast<assem::OperInstr *>(instr)) {
      if (oper_instr->jumps_) {
        const auto *jump_label_list = oper_instr->jumps_->labels_;
        auto *jump_node = instr_map_->Look(instr);
        for (const auto &jump_label : (*jump_label_list)) {
          auto *label_node = label_map_->Look(jump_label);
          flowgraph_->AddEdge(jump_node, label_node);
        }
      }
    }
  }
}

} // namespace fg

namespace assem {

std::set<temp::Temp *> remove_stack_pointer(std::set<temp::Temp *> temp_set) {
  temp_set.erase(reg_manager->StackPointer());
  return temp_set;
}

std::set<temp::Temp *> LabelInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return std::set<temp::Temp *>();
}

std::set<temp::Temp *> MoveInstr::Def() const {
  /* TODO: Put your lab6 code here */
  if (this->dst_ == nullptr)
    return std::set<temp::Temp *>();
  return remove_stack_pointer(this->dst_->ToSet());
}

std::set<temp::Temp *> OperInstr::Def() const {
  /* TODO: Put your lab6 code here */
  if (this->dst_ == nullptr)
    return std::set<temp::Temp *>();
  return remove_stack_pointer(this->dst_->ToSet());
}

std::set<temp::Temp *> LabelInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return std::set<temp::Temp *>();
}

std::set<temp::Temp *> MoveInstr::Use() const {
  /* TODO: Put your lab6 code here */
  if (this->src_ == nullptr)
    return std::set<temp::Temp *>();
  return remove_stack_pointer(this->src_->ToSet());
}

std::set<temp::Temp *> OperInstr::Use() const {
  /* TODO: Put your lab6 code here */
  if (this->src_ == nullptr)
    return std::set<temp::Temp *>();
  return remove_stack_pointer(this->src_->ToSet());
}
} // namespace assem
