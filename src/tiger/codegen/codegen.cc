#include "tiger/codegen/codegen.h"

#include <cassert>
#include <sstream>

extern frame::RegManager *reg_manager;

namespace {

constexpr int maxlen = 1024;

} // namespace

namespace cg {

bool can_be_scale_factor(int64_t value) {
  if (value == 1 || value == 2 || value == 4 || value == 8)
    return true;
  return false;
}

MemoryAddress calc_memory_address_index_mul_part(tree::Exp *index,
                                                 tree::Exp *scale) {
  auto *scale_const_exp = dynamic_cast<tree::ConstExp *>(scale);
  if (scale_const_exp == nullptr)
    return MemoryAddress();
  auto scale_consti = scale_const_exp->consti_;
  if (!can_be_scale_factor(scale_consti))
    return MemoryAddress();
  return MemoryAddress{MemoryAddressingMode::INDEX, 0, nullptr,
                       static_cast<int8_t>(scale_consti), index};
}

MemoryAddress calc_memory_address_index_part(tree::Exp *base, tree::Exp *mul) {
  auto *mul_binop_exp = dynamic_cast<tree::BinopExp *>(mul);
  if (mul_binop_exp == nullptr)
    return MemoryAddress();
  if (mul_binop_exp->op_ != tree::BinOp::MUL_OP)
    return MemoryAddress();
  auto *left = mul_binop_exp->left_;
  auto *right = mul_binop_exp->right_;
  auto res1 = calc_memory_address_index_mul_part(left, right);
  if (res1.mode != MemoryAddressingMode::INVALID) {
    res1.base = base;
    return res1;
  }
  auto res2 = calc_memory_address_index_mul_part(right, left);
  if (res2.mode != MemoryAddressingMode::INVALID) {
    res2.base = base;
    return res2;
  }
  return MemoryAddress();
}

MemoryAddress calc_memory_address_exp_without_const(tree::Exp *exp) {
  auto *binop_exp = dynamic_cast<tree::BinopExp *>(exp);
  if (binop_exp != nullptr) {
    if (binop_exp->op_ == tree::BinOp::MUL_OP) { // (, index, scale)
      auto *left = binop_exp->left_;
      auto *right = binop_exp->right_;
      auto res1 = calc_memory_address_index_mul_part(left, right);
      if (res1.mode != MemoryAddressingMode::INVALID)
        return res1;
      auto res2 = calc_memory_address_index_mul_part(right, left);
      if (res2.mode != MemoryAddressingMode::INVALID)
        return res2;
    }
    if (binop_exp->op_ == tree::BinOp::PLUS_OP) { // (base, index, scale)
      auto *left = binop_exp->left_;
      auto *right = binop_exp->right_;
      auto res1 = calc_memory_address_index_part(left, right);
      if (res1.mode != MemoryAddressingMode::INVALID)
        return res1;
      auto res2 = calc_memory_address_index_part(right, left);
      if (res2.mode != MemoryAddressingMode::INVALID)
        return res2;
    }
  }
  return MemoryAddress{MemoryAddressingMode::DISPLACEMENT, 0, exp, 0, nullptr};
}

MemoryAddress calc_memory_address_exp(tree::Exp *const_exp_, tree::Exp *exp_) {
  auto *const_exp = dynamic_cast<tree::ConstExp *>(const_exp_);
  if (const_exp == nullptr)
    return MemoryAddress();
  auto consti = const_exp->consti_;
  MemoryAddress res = calc_memory_address_exp_without_const(exp_);
  if (res.mode == MemoryAddressingMode::INVALID)
    return MemoryAddress(); // shouldn't happen
  res.value = consti;
  return res;
}

// pass in the exp inside mem exp
MemoryAddress calc_memory_address_exp(tree::Exp *exp) {
  auto *binop_exp = dynamic_cast<tree::BinopExp *>(exp);
  if (binop_exp != nullptr && binop_exp->op_ == tree::BinOp::PLUS_OP) {
    auto *left = binop_exp->left_;
    auto *right = binop_exp->right_;
    auto res1 = calc_memory_address_exp(left, right);
    if (res1.mode != MemoryAddressingMode::INVALID)
      return res1;
    auto res2 = calc_memory_address_exp(right, left);
    if (res2.mode != MemoryAddressingMode::INVALID)
      return res2;
  }
  return calc_memory_address_exp_without_const(exp);
}

std::string MemoryAddress::munch(assem::InstrList &instr_list,
                                 std::string_view fs, int &current_temp_idx,
                                 temp::TempList *temp_list) {
  std::string res;
  switch (mode) {
  case MemoryAddressingMode::DISPLACEMENT: {
    temp::Temp *base_temp = this->base->Munch(instr_list, fs);
    if (value != 0) {
      res = std::to_string(value) + "(`s" + std::to_string(current_temp_idx) +
            ")";
    } else {
      res = "(`s" + std::to_string(current_temp_idx) + ")";
    }
    temp_list->Append(base_temp);
    current_temp_idx++;
    break;
  }
  case MemoryAddressingMode::INDEX: {
    if (value != 0) {
      res = std::to_string(value);
    }
    res += "(";
    if (base != nullptr) {
      temp::Temp *base_temp = this->base->Munch(instr_list, fs);
      res += "`s" + std::to_string(current_temp_idx);
      temp_list->Append(base_temp);
      current_temp_idx++;
    }
    temp::Temp *index_temp = this->index->Munch(instr_list, fs);
    res += ",`s" + std::to_string(current_temp_idx) + "," +
           std::to_string(scale) + ")";
    temp_list->Append(index_temp);
    current_temp_idx++;
    break;
  }
  default:
    assert(false);
  }
  return res;
}

void CodeGen::Codegen() {
  /* TODO: Put your lab5 code here */
  assem::InstrList *instr_list = new assem::InstrList();
  std::string fs = frame_->name()->Name() + "_framesize";

  auto &callee_saves_list = reg_manager->CalleeSaves()->GetList();
  for (auto &reg : callee_saves_list) {
    temp::Temp *saved_temp = temp::TempFactory::NewTemp();
    saved_regs_.emplace_back(reg, saved_temp);
    instr_list->Append(new assem::MoveInstr("movq `s0, `d0",
                                            new temp::TempList(saved_temp),
                                            new temp::TempList(reg)));
  }

  auto &traces_stm_list = traces_->GetStmList()->GetList();
  for (auto &stm : traces_stm_list) {
    stm->Munch(*instr_list, fs);
  }

  for (auto it = saved_regs_.rbegin(); it != saved_regs_.rend(); it++) {
    instr_list->Append(new assem::MoveInstr("movq `s0, `d0",
                                            new temp::TempList(it->first),
                                            new temp::TempList(it->second)));
  }

  instr_list = frame_->procEntryExit2(instr_list);
  assem_instr_ = std::make_unique<AssemInstr>(instr_list);
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);
  fprintf(out, "\n");
}
} // namespace cg

namespace tree {
/* TODO: Put your lab5 code here */

void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  this->left_->Munch(instr_list, fs);
  this->right_->Munch(instr_list, fs);
}

void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(new assem::LabelInstr(
      temp::LabelFactory::LabelString(this->label_), this->label_));
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(new assem::OperInstr("jmp `j0", nullptr, nullptr,
                                         new assem::Targets(this->jumps_)));
}

void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  std::string op_str;
  switch (this->op_) {
  case RelOp::EQ_OP:
    op_str = "je";
    break;
  case RelOp::NE_OP:
    op_str = "jne";
    break;
  case RelOp::LT_OP:
    op_str = "jl";
    break;
  case RelOp::GT_OP:
    op_str = "jg";
    break;
  case RelOp::LE_OP:
    op_str = "jle";
    break;
  case RelOp::GE_OP:
    op_str = "jge";
    break;
  case RelOp::ULT_OP:
    op_str = "jb";
    break;
  case RelOp::UGT_OP:
    op_str = "ja";
    break;
  case RelOp::ULE_OP:
    op_str = "jbe";
    break;
  case RelOp::UGE_OP:
    op_str = "jae";
    break;
  default:
    assert(false);
  }
  temp::Temp *left = this->left_->Munch(instr_list, fs);
  temp::Temp *right = this->right_->Munch(instr_list, fs);
  instr_list.Append(new assem::OperInstr(
      "cmpq `s1, `s0", nullptr, new temp::TempList({left, right}), nullptr));
  instr_list.Append(
      new assem::OperInstr(op_str + " `j0", nullptr, nullptr,
                           new assem::Targets(new std::vector<temp::Label *>(
                               {this->true_label_, this->false_label_}))));
}

void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  auto *dst_mem_exp = dynamic_cast<MemExp *>(this->dst_);
  auto *src_mem_exp = dynamic_cast<MemExp *>(this->src_);
  if (dst_mem_exp != nullptr) {
    if (src_mem_exp != nullptr) { // mem to mem, needs a temp
      temp::Temp *src_temp = this->src_->Munch(instr_list, fs);
      cg::MemoryAddress dst_memory_address =
          cg::calc_memory_address_exp(dst_mem_exp->exp_);
      temp::TempList *assem_src_temp_list = new temp::TempList(src_temp);
      int current_temp_idx = 1;
      std::string dst_mem_str = dst_memory_address.munch(
          instr_list, fs, current_temp_idx, assem_src_temp_list);
      instr_list.Append(new assem::OperInstr(
          "movq `s0, " + dst_mem_str, nullptr, assem_src_temp_list, nullptr));
    } else {
      if (auto *src_const_exp = dynamic_cast<ConstExp *>(this->src_)) {
        // imm to mem
        cg::MemoryAddress dst_memory_address =
            cg::calc_memory_address_exp(dst_mem_exp->exp_);
        temp::TempList *assem_src_temp_list = new temp::TempList();
        int current_temp_idx = 0;
        std::string dst_mem_str = dst_memory_address.munch(
            instr_list, fs, current_temp_idx, assem_src_temp_list);
        instr_list.Append(new assem::OperInstr(
            "movq $" + std::to_string(src_const_exp->consti_) + ", " +
                dst_mem_str,
            nullptr, assem_src_temp_list, nullptr));
      } else {
        // reg to mem
        temp::Temp *src_temp = this->src_->Munch(instr_list, fs);
        cg::MemoryAddress dst_memory_address =
            cg::calc_memory_address_exp(dst_mem_exp->exp_);
        temp::TempList *assem_src_temp_list = new temp::TempList(src_temp);
        int current_temp_idx = 1;
        std::string dst_mem_str = dst_memory_address.munch(
            instr_list, fs, current_temp_idx, assem_src_temp_list);
        instr_list.Append(new assem::OperInstr(
            "movq `s0, " + dst_mem_str, nullptr, assem_src_temp_list, nullptr));
      }
    }
  } else {
    if (src_mem_exp != nullptr) {
      // mem to reg
      temp::TempList *assem_src_temp_list = new temp::TempList();
      int current_temp_idx = 0;
      cg::MemoryAddress src_memory_address =
          cg::calc_memory_address_exp(src_mem_exp->exp_);
      std::string src_mem_str = src_memory_address.munch(
          instr_list, fs, current_temp_idx, assem_src_temp_list);
      temp::Temp *dst_temp = this->dst_->Munch(instr_list, fs);
      instr_list.Append(new assem::OperInstr("movq " + src_mem_str + ", `d0",
                                             new temp::TempList(dst_temp),
                                             assem_src_temp_list, nullptr));
    } else if (auto *src_const_exp = dynamic_cast<ConstExp *>(this->src_)) {
      // imm to reg
      temp::Temp *dst_temp = this->dst_->Munch(instr_list, fs);
      instr_list.Append(new assem::OperInstr(
          "movq $" + std::to_string(src_const_exp->consti_) + ", `d0",
          new temp::TempList(dst_temp), nullptr, nullptr));
    } else {
      // reg to reg
      temp::Temp *src_temp = this->src_->Munch(instr_list, fs);
      temp::Temp *dst_temp = this->dst_->Munch(instr_list, fs);
      instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                             new temp::TempList(dst_temp),
                                             new temp::TempList(src_temp)));
    }
  }
}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  this->exp_->Munch(instr_list, fs);
}

temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *left_temp = this->left_->Munch(instr_list, fs);
  temp::Temp *right_temp = this->right_->Munch(instr_list, fs);
  temp::Temp *new_temp = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                         new temp::TempList(new_temp),
                                         new temp::TempList(left_temp)));
  std::string op_str;
  switch (this->op_) {
  case BinOp::PLUS_OP:
    instr_list.Append(new assem::OperInstr(
        "addq `s0, `d0", new temp::TempList(new_temp),
        new temp::TempList({right_temp, new_temp}), nullptr));
    break;
  case BinOp::MINUS_OP:
    instr_list.Append(new assem::OperInstr(
        "subq `s0, `d0", new temp::TempList(new_temp),
        new temp::TempList({right_temp, new_temp}), nullptr));
    break;
  case BinOp::MUL_OP:
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList(reg_manager->ReturnValue()),
        new temp::TempList(new_temp)));
    instr_list.Append(new assem::OperInstr(
        "imulq `s0", reg_manager->SpecialArithmaticOpRegs(),
        new temp::TempList({right_temp, reg_manager->ReturnValue()}), nullptr));
    instr_list.Append(
        new assem::MoveInstr("movq `s0, `d0", new temp::TempList(new_temp),
                             new temp::TempList(reg_manager->ReturnValue())));
    break;
  case BinOp::DIV_OP:
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList(reg_manager->ReturnValue()),
        new temp::TempList(new_temp)));
    instr_list.Append(new assem::OperInstr(
        "cqto", reg_manager->SpecialArithmaticOpRegs(),
        new temp::TempList(reg_manager->ReturnValue()), nullptr));
    instr_list.Append(new assem::OperInstr(
        "idivq `s0", reg_manager->SpecialArithmaticOpRegs(),
        new temp::TempList(right_temp), nullptr));
    instr_list.Append(
        new assem::MoveInstr("movq `s0, `d0", new temp::TempList(new_temp),
                             new temp::TempList(reg_manager->ReturnValue())));
    break;
  default:
    assert(0); // tiger programs shouldn't have other binop
  }
  return new_temp;
}

temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  cg::MemoryAddress memory_address = cg::calc_memory_address_exp(this->exp_);
  temp::TempList *assem_src_temp_list = new temp::TempList();
  int current_temp_idx = 0;
  std::string mem_str = memory_address.munch(instr_list, fs, current_temp_idx,
                                             assem_src_temp_list);
  temp::Temp *new_temp = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::OperInstr("movq " + mem_str + ", `d0",
                                         new temp::TempList(new_temp),
                                         assem_src_temp_list, nullptr));
  return new_temp;
}

temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  if (this->temp_ == reg_manager->FramePointer()) {
    temp::Temp *new_temp = temp::TempFactory::NewTemp();
    // leaq fs(%rsp), new_temp
    std::string leaq_str = std::string("leaq ") + fs.data() + "(`s0), `d0";
    instr_list.Append(new assem::OperInstr(
        leaq_str, new temp::TempList(new_temp),
        new temp::TempList(reg_manager->StackPointer()), nullptr));
    return new_temp;
  }
  return this->temp_;
}

temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  this->stm_->Munch(instr_list, fs);
  return this->exp_->Munch(instr_list, fs);
}

temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *new_temp = temp::TempFactory::NewTemp();
  instr_list.Append(
      new assem::OperInstr("leaq " + this->name_->Name() + "(%rip), `d0",
                           new temp::TempList(new_temp), nullptr, nullptr));
  return new_temp;
}

temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *new_temp = temp::TempFactory::NewTemp();
  instr_list.Append(
      new assem::OperInstr("movq $" + std::to_string(this->consti_) + ", `d0",
                           new temp::TempList(new_temp), nullptr, nullptr));
  return new_temp;
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  auto *fun_name_exp = dynamic_cast<NameExp *>(this->fun_);
  assert(fun_name_exp != nullptr);
  std::string fun_name = fun_name_exp->name_->Name();
  temp::TempList *args_temps = this->args_->MunchArgs(instr_list, fs);
  auto &args_temp_list = args_temps->GetList();
  int arg_cnt = 0;
  temp::TempList *arg_regs = reg_manager->ArgRegs();
  for (auto &arg_temp : args_temp_list) {
    if (arg_cnt < 6) {
      instr_list.Append(new assem::MoveInstr(
          "movq `s0, `d0", new temp::TempList(arg_regs->NthTemp(arg_cnt)),
          new temp::TempList(arg_temp)));
    } else {
      break;
    }
    arg_cnt++;
  }
  if (args_temp_list.size() > 6) {
    for (arg_cnt = args_temp_list.size() - 1; arg_cnt >= 6; arg_cnt--) {
      instr_list.Append(new assem::OperInstr(
          "subq $" + std::to_string(reg_manager->WordSize()) + ", `s0",
          new temp::TempList(reg_manager->StackPointer()),
          new temp::TempList(reg_manager->StackPointer()), nullptr));
      instr_list.Append(new assem::OperInstr(
          "movq `s0, (`s1)", nullptr,
          new temp::TempList(
              {args_temps->NthTemp(arg_cnt), reg_manager->StackPointer()}),nullptr));
    }
  }
  instr_list.Append(new assem::OperInstr(
      "callq " + fun_name, reg_manager->CallerSaves(), nullptr, nullptr));
  if (args_temp_list.size() > 6) {
    instr_list.Append(new assem::OperInstr(
        "addq $" +
            std::to_string(reg_manager->WordSize() *
                           (static_cast<int>(args_temp_list.size()) - 6)) +
            ", `s0",
        new temp::TempList(reg_manager->StackPointer()),
        new temp::TempList(reg_manager->StackPointer()), nullptr));
  }
  temp::Temp *new_temp = temp::TempFactory::NewTemp();
  instr_list.Append(
      new assem::MoveInstr("movq `s0, `d0", new temp::TempList(new_temp),
                           new temp::TempList(reg_manager->ReturnValue())));
  return new_temp;
}

temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list,
                                   std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::TempList *args_temps = new temp::TempList();
  for (auto &exp : this->exp_list_) {
    temp::Temp *temp = exp->Munch(instr_list, fs);
    args_temps->Append(temp);
  }
  return args_temps;
}

} // namespace tree
