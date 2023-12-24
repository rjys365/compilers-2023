#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"

extern frame::RegManager *reg_manager;

namespace ra {
/* TODO: Put your lab6 code here */
col::Result RegAllocator::AllocateOneRound() {
  auto flow_graph_factory = fg::FlowGraphFactory(instr_list_);
  auto flow_graph_ptr = flow_graph_factory.GetFlowGraph();
  auto live_graph_factory = live::LiveGraphFactory(flow_graph_ptr);
  auto live_graph = live_graph_factory.GetLiveGraph();
  auto color = col::Color(&live_graph);
  auto result = color.GetResult();
  return result;
}

void RegAllocator::RegAlloc() {
  // bool done = true;
  // remove_trivial_moves();
  while (true) {
    col::Result round_result = AllocateOneRound();
    if (!round_result.spills.empty()) {
      // done=false;
      rewrite_program(round_result.spills);
    } else {
      // auto *precolored_temp_map=reg_manager->temp_map_;
      auto *temp_map = temp::Map::Empty();
      for (const auto &temp : round_result.coloring) {
        std::string *s = new std::string(temp.second);
        temp_map->Enter(temp.first, s);
      }
      temp_map_ = temp_map;
      remove_trivial_moves();
      return;
    }
  }
}

void RegAllocator::rewrite_program(const std::list<temp::Temp *> &spills) {
  for (const auto &spilled_temp : spills) {
    auto *old_instr_list = instr_list_;
    instr_list_ = new assem::InstrList();
    auto offset = frame_->directlyAllocInFrameLocal();
    for (auto &instr : old_instr_list->GetList()) {
      const auto &def = instr->Def();
      const auto &use = instr->Use();
      if (def.count(spilled_temp) || use.count(spilled_temp)) {
        temp::TempList *new_src = new temp::TempList();
        temp::TempList *new_dst = new temp::TempList();
        temp::TempList *old_src = nullptr;
        temp::TempList *old_dst = nullptr;
        temp::Temp *spilled_src = nullptr;
        temp::Temp *spilled_dst = nullptr;
        assem::OperInstr *oper_instr = dynamic_cast<assem::OperInstr *>(instr);
        assem::MoveInstr *move_instr = dynamic_cast<assem::MoveInstr *>(instr);
        bool is_move = false;
        if (oper_instr != nullptr) {
          old_src = oper_instr->src_;
          old_dst = oper_instr->dst_;
        } else if (move_instr != nullptr) {
          old_src = move_instr->src_;
          old_dst = move_instr->dst_;
          is_move = true;
        } else {
          assert(false);
        }
        if (old_src == nullptr) {
          old_src = new temp::TempList();
        }
        if (old_dst == nullptr) {
          old_dst = new temp::TempList();
        }
        for (const auto &src_temp : old_src->GetList()) {
          if (src_temp == spilled_temp) {
            spilled_src = temp::TempFactory::NewTemp();
            break;
          }
        }
        for (const auto &dst_temp : old_dst->GetList()) {
          if (dst_temp == spilled_temp) {
            spilled_dst = temp::TempFactory::NewTemp();
            break;
          }
        }
        if (spilled_src != nullptr) {
          instr_list_->Append(new assem::OperInstr(
              "movq (" + frame_->name()->Name() + "_framesize-" +
                  std::to_string(-offset) + ")(`s0), `d0",
              new temp::TempList(spilled_src),
              new temp::TempList(reg_manager->StackPointer()), nullptr));
        }
        for (const auto &src_temp : old_src->GetList()) {
          if (src_temp != spilled_temp) {
            new_src->Append(src_temp);
          } else {
            new_src->Append(spilled_src);
          }
        }
        for (const auto &dst_temp : old_dst->GetList()) {
          if (dst_temp != spilled_temp) {
            new_dst->Append(dst_temp);
          } else {
            new_dst->Append(spilled_dst);
          }
        }
        if (is_move) {
          instr_list_->Append(
              new assem::MoveInstr("movq `s0, `d0", new_dst, new_src));
        } else {
          instr_list_->Append(new assem::OperInstr(
              oper_instr->assem_, new_dst, new_src, oper_instr->jumps_));
        }
        if (spilled_dst != nullptr) {
          instr_list_->Append(new assem::OperInstr(
              "movq `s0, (" + frame_->name()->Name() + "_framesize-" +
                  std::to_string(-offset) + ")(`s1)",
              nullptr,
              new temp::TempList({spilled_dst, reg_manager->StackPointer()}),
              nullptr));
        }
      } else {
        instr_list_->Append(instr);
      }
    }
    delete old_instr_list;
  }
}

void RegAllocator::remove_trivial_moves() {
  auto *old_instr_list = instr_list_;
  instr_list_ = new assem::InstrList();
  for (auto &instr : old_instr_list->GetList()) {
    auto *move_instr = dynamic_cast<assem::MoveInstr *>(instr);
    if (move_instr != nullptr) {
      const auto &src = move_instr->src_->GetList();
      const auto &dst = move_instr->dst_->GetList();
      assert(src.size() == 1 && dst.size() == 1);
      auto *string_src = temp_map_->Look(src.front());
      auto *string_dst = temp_map_->Look(dst.front());
      if (*string_src == *string_dst) {
        continue;
      }
    }
    instr_list_->Append(instr);
  }
}

} // namespace ra