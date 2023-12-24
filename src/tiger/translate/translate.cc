#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/x64frame.h"

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;

namespace tr {

Access *Access::AllocLocal(Level *level, bool escape) {
  /* TODO: Put your lab5 code here */
  return new Access(level, level->frame_->allocLocal(escape));
}

class Cx {
public:
  PatchList trues_;
  PatchList falses_;
  tree::Stm *stm_;

  Cx(PatchList trues, PatchList falses, tree::Stm *stm)
      : trues_(trues), falses_(falses), stm_(stm) {}
};

class Exp {
public:
  [[nodiscard]] virtual tree::Exp *UnEx() = 0;
  [[nodiscard]] virtual tree::Stm *UnNx() = 0;
  [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) = 0;
};

class ExpAndTy {
public:
  tr::Exp *exp_;
  type::Ty *ty_;

  ExpAndTy(tr::Exp *exp, type::Ty *ty) : exp_(exp), ty_(ty) {}
};

class ExExp : public Exp {
public:
  tree::Exp *exp_;

  explicit ExExp(tree::Exp *exp) : exp_(exp) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    /* TODO: Put your lab5 code here */
    return this->exp_;
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    return new tree::ExpStm(this->exp_);
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
    // special case for CONST(0/1)
    if (auto *const_exp = dynamic_cast<tree::ConstExp *>(this->exp_)) {
      auto labels_list = new std::vector<temp::Label *>();
      labels_list->push_back(nullptr);
      auto jump_stm =
          new tree::JumpStm(new tree::NameExp(nullptr), labels_list);
      std::list<temp::Label **> list_jump_label;
      list_jump_label.push_back(&jump_stm->exp_->name_);
      list_jump_label.push_back(&(*labels_list)[0]);
      if (const_exp->consti_ == 0) {
        return Cx(PatchList(), PatchList(list_jump_label), jump_stm);
      } else {
        return Cx(PatchList(list_jump_label), PatchList(), jump_stm);
      }
    }

    auto cjump_stm =
        new tree::CjumpStm(tree::RelOp::NE_OP, this->exp_,
                           new tree::ConstExp(0), nullptr, nullptr);
    std::list<temp::Label **> list_true, list_false;
    list_true.push_back(&cjump_stm->true_label_);
    list_false.push_back(&cjump_stm->false_label_);

    return Cx(PatchList(list_true), PatchList(list_false), cjump_stm);
  }
};

class NxExp : public Exp {
public:
  tree::Stm *stm_;

  explicit NxExp(tree::Stm *stm) : stm_(stm) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    /* TODO: Put your lab5 code here */
    return new tree::EseqExp(this->stm_, new tree::ConstExp(0));
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    return this->stm_;
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
    errormsg->Error(0, "Should not call UnCx for an NxExp");
  }
};

class CxExp : public Exp {
public:
  Cx cx_;

  CxExp(PatchList trues, PatchList falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    /* TODO: Put your lab5 code here */
    temp::Temp *r = temp::TempFactory::NewTemp();
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();
    this->cx_.trues_.DoPatch(t);
    this->cx_.falses_.DoPatch(f);
    return new tree::EseqExp(
        new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(1)),
        new tree::EseqExp(
            this->cx_.stm_,
            new tree::EseqExp(
                new tree::LabelStm(f),
                new tree::EseqExp(new tree::MoveStm(new tree::TempExp(r),
                                                    new tree::ConstExp(0)),
                                  new tree::EseqExp(new tree::LabelStm(t),
                                                    new tree::TempExp(r))))));
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    temp::Label *end = temp::LabelFactory::NewLabel();
    this->cx_.trues_.DoPatch(end);
    this->cx_.falses_.DoPatch(end);
    return new tree::SeqStm(this->cx_.stm_, new tree::LabelStm(end));
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
    return this->cx_;
  }
};

void ProgTr::Translate() { /* TODO: Put your lab5 code here */
  auto *root_exp_ty = this->absyn_tree_->Translate(
      this->venv_.get(), this->tenv_.get(), this->main_level_.get(), nullptr,
      this->errormsg_.get());
  tree::Stm *stm = root_exp_ty->exp_->UnNx();
  stm = this->main_level_->frame_->procEntryExit1(stm);
  frags->PushBack(new frame::ProcFrag(stm,
                                      this->main_level_->frame_));
}

tree::Exp *calc_static_link(tr::Level *current_level, tr::Level *dest_level,
                            tree::Exp *frame_pointer) {
  while (current_level != dest_level) {
    if (current_level == nullptr)
      return nullptr;
    frame_pointer = new tree::MemExp(new tree::BinopExp(
        tree::BinOp::MINUS_OP, frame_pointer,
        new tree::ConstExp(reg_manager->WordSize()))); // TODO:fp-8?
    current_level = current_level->parent_;
  }
  return frame_pointer;
}

} // namespace tr

namespace absyn {

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return root_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  env::EnvEntry *entry = venv->Look(this->sym_);
  env::VarEntry *varEntry = dynamic_cast<env::VarEntry *>(entry);
  assert(varEntry != nullptr);

  tr::Access *access = varEntry->access_;
  tr::Level *current_level = level, *dest_level = access->level_;
  tree::Exp *frame_pointer =
      tr::calc_static_link(current_level, dest_level,
                           new tree::TempExp(reg_manager->FramePointer()));
  return new tr::ExpAndTy(new tr::ExExp(access->access_->Exp(frame_pointer)),
                          varEntry->ty_->ActualTy());
}

tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *var_exp_ty =
      var_->Translate(venv, tenv, level, label, errormsg);
  auto *var_exp = var_exp_ty->exp_;
  auto *var_ty = var_exp_ty->ty_;
  auto *var_record_ty = dynamic_cast<type::RecordTy *>(var_ty->ActualTy());
  assert(var_record_ty != nullptr);
  const auto &fields_list = var_record_ty->fields_->GetList();
  int offset = 0;
  type::Ty *field_ty = nullptr;
  for (const auto &field : fields_list) {
    if (field->name_ == sym_) {
      field_ty = field->ty_;
      break;
    }
    offset += reg_manager->WordSize();
  }
  assert(field_ty != nullptr);
  return new tr::ExpAndTy(
      new tr::ExExp(new tree::MemExp(new tree::BinopExp(
          tree::BinOp::PLUS_OP, var_exp->UnEx(), new tree::ConstExp(offset)))),
      field_ty->ActualTy());
}

tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level, temp::Label *label,
                                      err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *var_exp_ty =
      var_->Translate(venv, tenv, level, label, errormsg);
  auto *var_exp = var_exp_ty->exp_;
  auto *var_ty = var_exp_ty->ty_;
  auto *var_array_ty = dynamic_cast<type::ArrayTy *>(var_ty->ActualTy());
  assert(var_array_ty != nullptr);
  tr::ExpAndTy *subscript_exp_ty =
      subscript_->Translate(venv, tenv, level, label, errormsg);
  return new tr::ExpAndTy(
      new tr::ExExp(new tree::MemExp(new tree::BinopExp(
          tree::BinOp::PLUS_OP, var_exp->UnEx(),
          new tree::BinopExp(tree::BinOp::MUL_OP,
                             subscript_exp_ty->exp_->UnEx(),
                             new tree::ConstExp(reg_manager->WordSize()))))),
      var_array_ty->ty_->ActualTy());
}

tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return var_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                          type::NilTy::Instance());
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(val_)),
                          type::IntTy::Instance());
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto *new_label = temp::LabelFactory::NewLabel();
  frags->PushBack(new frame::StringFrag(new_label, str_));
  return new tr::ExpAndTy(new tr::ExExp(new tree::NameExp(new_label)),
                          type::StringTy::Instance());
}

tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto *entry = venv->Look(this->func_);
  auto *fun_entry = dynamic_cast<env::FunEntry *>(entry);
  assert(fun_entry != nullptr);

  bool is_library_func = (fun_entry->level_->parent_ == nullptr);

  const auto &args_list = this->args_->GetList();
  tree::ExpList *tree_args_list = new tree::ExpList();

  if (!is_library_func) {
    tree::Exp *fp = new tree::TempExp(reg_manager->FramePointer());
    tr::Level *current_level = level;
    tr::Level *dest_level = fun_entry->level_->parent_;
    tree::Exp *static_link =
        tr::calc_static_link(current_level, dest_level, fp);
    tree_args_list->Append(static_link);
  }

  for (const auto &arg : args_list) {
    auto *arg_exp_ty = arg->Translate(venv, tenv, level, label, errormsg);
    tree_args_list->Append(arg_exp_ty->exp_->UnEx());
  }

  tree::Exp *res_exp;
  type::Ty *res_ty = type::VoidTy::Instance();
  if (fun_entry->result_ != nullptr) {
    res_ty = fun_entry->result_->ActualTy();
  }

  if (is_library_func) {
    res_exp = level->frame_->externalCall(
        temp::LabelFactory::LabelString(this->func_), tree_args_list);
  } else {
    res_exp =
        new tree::CallExp(new tree::NameExp(fun_entry->label_), tree_args_list);
  }

  return new tr::ExpAndTy(new tr::ExExp(res_exp), res_ty);
}

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *left_exp_ty =
      this->left_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *right_exp_ty =
      this->right_->Translate(venv, tenv, level, label, errormsg);

  tree::Exp *left_exp = left_exp_ty->exp_->UnEx();
  tree::Exp *right_exp = right_exp_ty->exp_->UnEx();

  type::Ty *left_ty = left_exp_ty->ty_;
  type::Ty *right_ty = right_exp_ty->ty_;

  tr::Exp *res_exp;
  type::Ty *res_ty = type::IntTy::Instance();

  switch (this->oper_) {
  case Oper::PLUS_OP: {
    res_exp = new tr::ExExp(
        new tree::BinopExp(tree::BinOp::PLUS_OP, left_exp, right_exp));
    break;
  }
  case Oper::MINUS_OP: {
    res_exp = new tr::ExExp(
        new tree::BinopExp(tree::BinOp::MINUS_OP, left_exp, right_exp));
    break;
  }
  case Oper::TIMES_OP: {
    res_exp = new tr::ExExp(
        new tree::BinopExp(tree::BinOp::MUL_OP, left_exp, right_exp));
    break;
  }
  case Oper::DIVIDE_OP: {
    res_exp = new tr::ExExp(
        new tree::BinopExp(tree::BinOp::DIV_OP, left_exp, right_exp));
    break;
  }
  case Oper::EQ_OP: {
    tree::CjumpStm *cjump_stm;
    if (left_ty->IsSameType(type::StringTy::Instance()) &&
        right_ty->IsSameType(type::StringTy::Instance())) {
      // String compare
      auto *exp_list = new tree::ExpList();
      exp_list->Append(left_exp);
      exp_list->Append(right_exp);
      cjump_stm = new tree::CjumpStm(
          tree::RelOp::EQ_OP,
          level->frame_->externalCall("string_equal", exp_list),
          new tree::ConstExp(1), nullptr, nullptr);
    } else {
      cjump_stm = new tree::CjumpStm(tree::RelOp::EQ_OP, left_exp, right_exp,
                                     nullptr, nullptr);
    }
    res_exp = new tr::CxExp(tr::PatchList(&cjump_stm->true_label_),
                            tr::PatchList(&cjump_stm->false_label_), cjump_stm);
    break;
  }
  case Oper::NEQ_OP: {
    tree::CjumpStm *cjump_stm = new tree::CjumpStm(tree::RelOp::NE_OP, left_exp,
                                                   right_exp, nullptr, nullptr);
    res_exp = new tr::CxExp(tr::PatchList(&cjump_stm->true_label_),
                            tr::PatchList(&cjump_stm->false_label_), cjump_stm);
    break;
  }
  case Oper::LT_OP: {
    tree::CjumpStm *cjump_stm = new tree::CjumpStm(tree::RelOp::LT_OP, left_exp,
                                                   right_exp, nullptr, nullptr);
    res_exp = new tr::CxExp(tr::PatchList(&cjump_stm->true_label_),
                            tr::PatchList(&cjump_stm->false_label_), cjump_stm);
    break;
  }
  case Oper::LE_OP: {
    tree::CjumpStm *cjump_stm = new tree::CjumpStm(tree::RelOp::LE_OP, left_exp,
                                                   right_exp, nullptr, nullptr);
    res_exp = new tr::CxExp(tr::PatchList(&cjump_stm->true_label_),
                            tr::PatchList(&cjump_stm->false_label_), cjump_stm);
    break;
  }
  case Oper::GT_OP: {
    tree::CjumpStm *cjump_stm = new tree::CjumpStm(tree::RelOp::GT_OP, left_exp,
                                                   right_exp, nullptr, nullptr);
    res_exp = new tr::CxExp(tr::PatchList(&cjump_stm->true_label_),
                            tr::PatchList(&cjump_stm->false_label_), cjump_stm);
    break;
  }
  case Oper::GE_OP: {
    tree::CjumpStm *cjump_stm = new tree::CjumpStm(tree::RelOp::GE_OP, left_exp,
                                                   right_exp, nullptr, nullptr);
    res_exp = new tr::CxExp(tr::PatchList(&cjump_stm->true_label_),
                            tr::PatchList(&cjump_stm->false_label_), cjump_stm);
    break;
  }
  case Oper::AND_OP: {
    tr::Cx left_cx = left_exp_ty->exp_->UnCx(errormsg);
    tr::Cx right_cx = right_exp_ty->exp_->UnCx(errormsg);
    // Logger(stdout).Log(left_cx.stm_);
    temp::Label *true_label_1 = temp::LabelFactory::NewLabel();
    temp::Label *true_label_2 = temp::LabelFactory::NewLabel();
    temp::Label *false_label = temp::LabelFactory::NewLabel();
    temp::Label *end_label = temp::LabelFactory::NewLabel();
    temp::Temp *r = temp::TempFactory::NewTemp();
    left_cx.trues_.DoPatch(true_label_1);
    right_cx.trues_.DoPatch(true_label_2);
    left_cx.falses_.DoPatch(false_label);
    right_cx.falses_.DoPatch(false_label);
    auto *eseq = new tree::EseqExp(
        new tree::SeqStm(
            left_cx.stm_,
            new tree::SeqStm(
                new tree::LabelStm(true_label_1),
                new tree::SeqStm(
                    right_cx.stm_,
                    new tree::SeqStm(
                        new tree::LabelStm(true_label_2),
                        new tree::SeqStm(
                            new tree::MoveStm(new tree::TempExp(r),
                                              new tree::ConstExp(1)),
                            new tree::SeqStm(
                                new tree::JumpStm(
                                    new tree::NameExp(end_label),
                                    new std::vector<temp::Label *>(
                                        {end_label})),
                                new tree::SeqStm(
                                    new tree::LabelStm(false_label),
                                    new tree::SeqStm(
                                        new tree::MoveStm(
                                            new tree::TempExp(r),
                                            new tree::ConstExp(0)),
                                        new tree::LabelStm(end_label))))))))),
        new tree::TempExp(r));
    // Logger().Log(new tree::ExpStm(eseq));
    res_exp = new tr::ExExp(eseq);
    break;
  }
  case Oper::OR_OP: {
    tr::Cx left_cx = left_exp_ty->exp_->UnCx(errormsg);
    tr::Cx right_cx = right_exp_ty->exp_->UnCx(errormsg);
    temp::Label *true_label = temp::LabelFactory::NewLabel();
    temp::Label *false_label_1 = temp::LabelFactory::NewLabel();
    temp::Label *false_label_2 = temp::LabelFactory::NewLabel();
    temp::Label *end_label = temp::LabelFactory::NewLabel();
    temp::Temp *r = temp::TempFactory::NewTemp();
    left_cx.trues_.DoPatch(true_label);
    right_cx.trues_.DoPatch(true_label);
    left_cx.falses_.DoPatch(false_label_1);
    right_cx.falses_.DoPatch(false_label_2);
    auto *eseq = new tree::EseqExp(
        new tree::SeqStm(
            left_cx.stm_,
            new tree::SeqStm(
                new tree::LabelStm(false_label_1),
                new tree::SeqStm(
                    right_cx.stm_,
                    new tree::SeqStm(
                        new tree::LabelStm(false_label_2),
                        new tree::SeqStm(
                            new tree::MoveStm(new tree::TempExp(r),
                                              new tree::ConstExp(0)),
                            new tree::SeqStm(
                                new tree::JumpStm(
                                    new tree::NameExp(end_label),
                                    new std::vector<temp::Label *>(
                                        {end_label})),
                                new tree::SeqStm(
                                    new tree::LabelStm(true_label),
                                    new tree::SeqStm(
                                        new tree::MoveStm(
                                            new tree::TempExp(r),
                                            new tree::ConstExp(1)),
                                        new tree::LabelStm(end_label))))))))),
        new tree::TempExp(r));
    res_exp = new tr::ExExp(eseq);
    break;
  }
  default:
    assert(0); // shouldn't happen
    break;
  }
  return new tr::ExpAndTy(res_exp, res_ty);
}

tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto *ty = tenv->Look(this->typ_);
  auto *record_ty = dynamic_cast<type::RecordTy *>(ty->ActualTy());
  assert(record_ty != nullptr);

  temp::Temp *record_temp = temp::TempFactory::NewTemp();

  const auto &efield_list = fields_->GetList();
  int record_size = efield_list.size() * reg_manager->WordSize();

  tree::ExpList *alloc_args_list = new tree::ExpList();
  alloc_args_list->Append(new tree::ConstExp(record_size));

  tree::Stm *stm_seq = new tree::MoveStm(
      new tree::TempExp(record_temp),
      level->frame_->externalCall("alloc_record", alloc_args_list));

  int field_cnt = 0;
  for (const auto &efield : efield_list) {
    auto *efield_exp_ty =
        efield->exp_->Translate(venv, tenv, level, label, errormsg);
    stm_seq = new tree::SeqStm(
        stm_seq,
        new tree::MoveStm(
            new tree::MemExp(new tree::BinopExp(
                tree::BinOp::PLUS_OP, new tree::TempExp(record_temp),
                new tree::ConstExp(field_cnt * reg_manager->WordSize()))),
            efield_exp_ty->exp_->UnEx()));
    field_cnt++;
  }

  tr::ExExp *res_exp =
      new tr::ExExp(new tree::EseqExp(stm_seq, new tree::TempExp(record_temp)));
  return new tr::ExpAndTy(res_exp, record_ty);
}

tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  const auto &exp_list = this->seq_->GetList();
  tree::Exp *eseq_exp = nullptr;
  type::Ty *last_ty = nullptr;
  for (const auto &exp : exp_list) {
    auto *exp_ty = exp->Translate(venv, tenv, level, label, errormsg);
    if (eseq_exp == nullptr) {
      eseq_exp = exp_ty->exp_->UnEx();
    } else {
      eseq_exp =
          new tree::EseqExp(new tree::ExpStm(eseq_exp), exp_ty->exp_->UnEx());
    }
    last_ty = exp_ty->ty_;
  }
  return new tr::ExpAndTy(new tr::ExExp(eseq_exp), last_ty);
}

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto *var_exp_ty = var_->Translate(venv, tenv, level, label, errormsg);
  auto *var_exp = var_exp_ty->exp_->UnEx();

  auto *exp_exp_ty = exp_->Translate(venv, tenv, level, label, errormsg);
  auto *exp_exp = exp_exp_ty->exp_->UnEx();

  tree::MoveStm *move_stm = new tree::MoveStm(var_exp, exp_exp);
  return new tr::ExpAndTy(new tr::NxExp(move_stm), type::VoidTy::Instance());
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto *test_exp_ty = test_->Translate(venv, tenv, level, label, errormsg);
  auto *then_exp_ty = then_->Translate(venv, tenv, level, label, errormsg);

  tr::Exp *res_exp;
  type::Ty *res_ty;

  if (elsee_ != nullptr) {
    auto *else_exp_ty = elsee_->Translate(venv, tenv, level, label, errormsg);
    tr::Cx test_cx = test_exp_ty->exp_->UnCx(errormsg);
    temp::Label *true_label = temp::LabelFactory::NewLabel();
    temp::Label *false_label = temp::LabelFactory::NewLabel();
    temp::Label *end_label = temp::LabelFactory::NewLabel();
    test_cx.trues_.DoPatch(true_label);
    test_cx.falses_.DoPatch(false_label);

    temp::Temp *r = temp::TempFactory::NewTemp();
    res_exp = new tr::ExExp(new tree::EseqExp(
        test_cx.stm_,
        new tree::EseqExp(
            new tree::LabelStm(true_label),
            new tree::EseqExp(
                new tree::MoveStm(new tree::TempExp(r),
                                  then_exp_ty->exp_->UnEx()),
                new tree::EseqExp(
                    new tree::JumpStm(
                        new tree::NameExp(end_label),
                        new std::vector<temp::Label *>{end_label}),
                    new tree::EseqExp(
                        new tree::LabelStm(false_label),
                        new tree::EseqExp(
                            new tree::MoveStm(new tree::TempExp(r),
                                              else_exp_ty->exp_->UnEx()),
                            new tree::EseqExp(new tree::LabelStm(end_label),
                                              new tree::TempExp(r)))))))));
    res_ty = then_exp_ty->ty_;
  } else {
    tr::Cx test_cx = test_exp_ty->exp_->UnCx(errormsg);
    temp::Label *true_label = temp::LabelFactory::NewLabel();
    temp::Label *false_label = temp::LabelFactory::NewLabel();
    test_cx.trues_.DoPatch(true_label);
    test_cx.falses_.DoPatch(false_label);

    res_exp = new tr::NxExp(new tree::SeqStm(
        test_cx.stm_,
        new tree::SeqStm(new tree::LabelStm(true_label),
                         new tree::SeqStm(then_exp_ty->exp_->UnNx(),
                                          new tree::LabelStm(false_label)))));
    res_ty = type::VoidTy::Instance();
  }
  return new tr::ExpAndTy(res_exp, res_ty);
}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto *test_exp_ty = test_->Translate(venv, tenv, level, label, errormsg);
  temp::Label *test_label = temp::LabelFactory::NewLabel();
  temp::Label *body_start_label = temp::LabelFactory::NewLabel();
  temp::Label *end_label = temp::LabelFactory::NewLabel();
  auto *body_exp_ty = body_->Translate(venv, tenv, level, end_label, errormsg);

  tr::Cx test_cx = test_exp_ty->exp_->UnCx(errormsg);
  test_cx.trues_.DoPatch(body_start_label);
  test_cx.falses_.DoPatch(end_label);

  tree::Stm *stm_seq = new tree::SeqStm(
      new tree::LabelStm(test_label),
      new tree::SeqStm(
          test_cx.stm_,
          new tree::SeqStm(
              new tree::LabelStm(body_start_label),
              new tree::SeqStm(
                  body_exp_ty->exp_->UnNx(),
                  new tree::SeqStm(
                      new tree::JumpStm(
                          new tree::NameExp(test_label),
                          new std::vector<temp::Label *>{test_label}),
                      new tree::LabelStm(end_label))))));
  return new tr::ExpAndTy(new tr::NxExp(stm_seq), type::VoidTy::Instance());
}

tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto *lo_exp_ty = lo_->Translate(venv, tenv, level, label, errormsg);
  auto *hi_exp_ty = hi_->Translate(venv, tenv, level, label, errormsg);

  temp::Label *test_label = temp::LabelFactory::NewLabel();
  temp::Label *body_start_label = temp::LabelFactory::NewLabel();
  temp::Label *end_label = temp::LabelFactory::NewLabel();

  venv->BeginScope();

  auto *loop_var_entry = new env::VarEntry(tr::Access::AllocLocal(level, false),
                                           type::IntTy::Instance(), true);
  venv->Enter(var_, loop_var_entry);

  auto *loop_var_exp = loop_var_entry->access_->access_->Exp(
      new tree::TempExp(reg_manager->FramePointer()));
  auto *limit_var_temp = temp::TempFactory::NewTemp();
  auto *limit_var_exp = new tree::TempExp(limit_var_temp);
  auto *body_exp_ty = body_->Translate(venv, tenv, level, end_label, errormsg);
  venv->EndScope();

  tree::Stm *stm_seq = new tree::SeqStm(
      new tree::MoveStm(loop_var_exp, lo_exp_ty->exp_->UnEx()),
      new tree::SeqStm(
          new tree::MoveStm(limit_var_exp, hi_exp_ty->exp_->UnEx()),
          new tree::SeqStm(
              new tree::LabelStm(test_label),
              new tree::SeqStm(
                  new tree::CjumpStm(tree::RelOp::LE_OP, loop_var_exp,
                                     limit_var_exp, body_start_label,
                                     end_label),
                  new tree::SeqStm(
                      new tree::LabelStm(body_start_label),
                      new tree::SeqStm(
                          body_exp_ty->exp_->UnNx(),
                          new tree::SeqStm(
                              new tree::MoveStm(
                                  loop_var_exp,
                                  new tree::BinopExp(tree::BinOp::PLUS_OP,
                                                     loop_var_exp,
                                                     new tree::ConstExp(1))),
                              new tree::SeqStm(
                                  new tree::JumpStm(
                                      new tree::NameExp(test_label),
                                      new std::vector<temp::Label *>{
                                          test_label}),
                                  new tree::LabelStm(end_label)))))))));
  return new tr::ExpAndTy(new tr::NxExp(stm_seq), type::VoidTy::Instance());
}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(
      new tr::NxExp(new tree::JumpStm(new tree::NameExp(label),
                                      new std::vector<temp::Label *>{label})),
      type::VoidTy::Instance());
}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  venv->BeginScope();
  tenv->BeginScope();

  const auto &decs_list = this->decs_->GetList();
  tree::Stm *stm_seq = nullptr;
  for (const auto &dec : decs_list) {
    auto *dec_exp = dec->Translate(venv, tenv, level, label, errormsg);
    if (stm_seq == nullptr) {
      stm_seq = dec_exp->UnNx();
    } else {
      stm_seq = new tree::SeqStm(stm_seq, dec_exp->UnNx());
    }
  }

  auto *body_exp_ty = body_->Translate(venv, tenv, level, label, errormsg);
  venv->EndScope();
  tenv->EndScope();

  if (stm_seq == nullptr) {
    return body_exp_ty; // shouldn't happen?
  } else {
    return new tr::ExpAndTy(
        new tr::ExExp(new tree::EseqExp(stm_seq, body_exp_ty->exp_->UnEx())),
        body_exp_ty->ty_);
  }
}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto *size_exp_ty = size_->Translate(venv, tenv, level, label, errormsg);
  auto *init_exp_ty = init_->Translate(venv, tenv, level, label, errormsg);

  auto *alloc_args_list = new tree::ExpList();
  alloc_args_list->Append(
      new tree::BinopExp(tree::BinOp::MUL_OP, size_exp_ty->exp_->UnEx(),
                         new tree::ConstExp(reg_manager->WordSize())));
  alloc_args_list->Append(init_exp_ty->exp_->UnEx());

  tree::Exp *alloc_exp =
      level->frame_->externalCall("init_array", alloc_args_list);
  return new tr::ExpAndTy(new tr::ExExp(alloc_exp),
                          new type::ArrayTy(init_exp_ty->ty_));
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                          type::VoidTy::Instance());
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  const auto &dec_list = this->functions_->GetList();

  for (const auto &dec : dec_list) {
    // there shouldn't be repeated definition of function
    std::list<bool> escapes;

    const auto &params_list = dec->params_->GetList();
    for (const auto &param : params_list) {
      escapes.push_back(param->escape_);
    }
    type::TyList *formal_tys = dec->params_->MakeFormalTyList(tenv, errormsg);
    auto *new_level = new tr::Level(level, dec->name_, escapes);
    if (dec->result_) {
      auto *result_ty = tenv->Look(dec->result_);
      venv->Enter(dec->name_, new env::FunEntry(new_level, dec->name_,
                                                formal_tys, result_ty));
    } else {
      venv->Enter(dec->name_,
                  new env::FunEntry(new_level, dec->name_, formal_tys,
                                    type::VoidTy::Instance()));
    }
  }

  for (const auto &dec : dec_list) {
    auto *fun_entry = dynamic_cast<env::FunEntry *>(venv->Look(dec->name_));
    assert(fun_entry != nullptr);
    auto formal_ty_list =
        dec->params_->MakeFormalTyList(tenv, errormsg)->GetList();

    auto &param_list = dec->params_->GetList();
    auto &formals_access_list = fun_entry->level_->frame_->formals();

    auto ty_it = formal_ty_list.begin();
    auto param_it = param_list.begin();
    auto access_it = std::next(formals_access_list.begin()); // skip static link

    venv->BeginScope();

    for (; param_it != param_list.end(); ty_it++, param_it++, access_it++) {
      venv->Enter(
          (*param_it)->name_,
          new env::VarEntry(new tr::Access(fun_entry->level_, *access_it),
                            *ty_it, false));
    }

    tr::ExpAndTy *body_exp_ty =
        dec->body_->Translate(venv, tenv, fun_entry->level_, label, errormsg);
    venv->EndScope();

    tree::Stm *stm;

    if (fun_entry->result_ != nullptr) {
      stm = new tree::MoveStm(new tree::TempExp(reg_manager->ReturnValue()),
                              body_exp_ty->exp_->UnEx());
    } else {
      stm = body_exp_ty->exp_->UnNx();
    }

    stm = fun_entry->level_->frame_->procEntryExit1(stm);

    frags->PushBack(new frame::ProcFrag(stm, fun_entry->level_->frame_));
  }

  return new tr::ExExp(new tree::ConstExp(0));
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto *init_exp_ty = init_->Translate(venv, tenv, level, label, errormsg);
  auto *init_ty = init_exp_ty->ty_->ActualTy();

  if (typ_) {
    init_ty = tenv->Look(typ_)->ActualTy();
  }

  auto *access = tr::Access::AllocLocal(level, escape_);
  venv->Enter(var_, new env::VarEntry(access, init_ty, false));

  // just declared this variable, no need to calc static link
  tree::Exp *access_exp =
      access->access_->Exp(new tree::TempExp(reg_manager->FramePointer()));

  return new tr::NxExp(
      new tree::MoveStm(access_exp, init_exp_ty->exp_->UnEx()));
}

tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  const auto &dec_list = this->types_->GetList();

  for (const auto &dec : dec_list) {
    // there shouldn't be repeated definition of type
    tenv->Enter(dec->name_, new type::NameTy(dec->name_, nullptr));
  }

  for (const auto &dec : dec_list) {
    auto *ty = tenv->Look(dec->name_);
    auto *name_ty = dynamic_cast<type::NameTy *>(ty);
    assert(name_ty != nullptr);
    name_ty->ty_ = dec->ty_->Translate(tenv, errormsg);
  }

  // there shouldn't be cirles in type definition

  return new tr::ExExp(new tree::ConstExp(0));
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::Ty *ty = tenv->Look(this->name_);
  assert(ty != nullptr);
  return new type::NameTy(this->name_, ty);
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new type::RecordTy(this->record_->MakeFieldList(tenv, errormsg));
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::Ty *ty = tenv->Look(this->array_);
  assert(ty != nullptr);
  return new type::ArrayTy(ty);
}

} // namespace absyn
