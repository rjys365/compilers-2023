#include "tiger/semant/semant.h"
#include "tiger/absyn/absyn.h"

namespace absyn {

void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  this->root_->SemAnalyze(venv, tenv, 0, errormsg);
}

type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  env::EnvEntry *entry = venv->Look(this->sym_);
  env::VarEntry *varEntry = dynamic_cast<env::VarEntry *>(entry);
  if (varEntry != nullptr) {
    return varEntry->ty_->ActualTy();
  } else {
    errormsg->Error(this->pos_, "undefined variable %s",
                    this->sym_->Name().c_str());
    return type::IntTy::Instance();
  }
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = this->var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::RecordTy *recordTy = dynamic_cast<type::RecordTy *>(ty);
  if (!recordTy) {
    errormsg->Error(this->pos_, "not a record type");
    return type::IntTy::Instance();
  }
  const std::list<type::Field *> &fieldList = recordTy->fields_->GetList();
  for (auto iter = fieldList.cbegin(); iter != fieldList.cend(); iter++) {
    if ((*iter)->name_ == this->sym_)
      return (*iter)->ty_;
  }
  errormsg->Error(this->pos_, "field %s doesn't exist",
                  this->sym_->Name().c_str());
  return type::IntTy::Instance();
}

type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = this->var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::ArrayTy *arrayTy = dynamic_cast<type::ArrayTy *>(ty);
  if (!arrayTy) {
    errormsg->Error(this->pos_, "array type required");
    return type::IntTy::Instance();
  }

  type::Ty *subscriptTy =
      this->subscript_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!dynamic_cast<type::IntTy *>(subscriptTy)) {
    errormsg->Error(this->pos_, "array index must be integer");
  }

  return arrayTy->ty_->ActualTy();
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return this->var_->SemAnalyze(venv, tenv, labelcount, errormsg);
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::StringTy::Instance();
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  env::EnvEntry *entry = venv->Look(this->func_);
  env::FunEntry *funEntry = dynamic_cast<env::FunEntry *>(entry);
  if (!funEntry) {
    errormsg->Error(this->pos_, "undefined function %s",
                    this->func_->Name().c_str());
    return type::IntTy::Instance();
  }

  auto &formalsList = funEntry->formals_->GetList();
  auto &actualsList = this->args_->GetList();

  auto formalsIter = formalsList.cbegin();
  auto actualsIter = actualsList.cbegin();

  while (formalsIter != formalsList.cend() &&
         actualsIter != actualsList.cend()) {
    type::Ty *actualTy =
        (*actualsIter)->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!actualTy->IsSameType(*formalsIter)) {
      errormsg->Error(this->pos_, "para type mismatch");
      return type::IntTy::Instance();
    }
    formalsIter++;
    actualsIter++;
  }

  if (formalsList.size() > actualsList.size()) {
    errormsg->Error(this->pos_, "too few params in function %s",
                    this->func_->Name().c_str());
    return type::IntTy::Instance();
  }
  if (formalsList.size() < actualsList.size()) {
    errormsg->Error(this->pos_, "too many params in function %s",
                    this->func_->Name().c_str());
    return type::IntTy::Instance();
  }

  return funEntry->result_->ActualTy();
}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  absyn::Oper oper = this->oper_;
  type::Ty *leftTy = this->left_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *rightTy =
      this->right_->SemAnalyze(venv, tenv, labelcount, errormsg);
  switch (oper) {
  case absyn::Oper::PLUS_OP:
  case absyn::Oper::MINUS_OP:
  case absyn::Oper::TIMES_OP:
  case absyn::Oper::DIVIDE_OP: {
    if (!leftTy->IsSameType(type::IntTy::Instance()) ||
        !rightTy->IsSameType(type::IntTy::Instance())) {
      errormsg->Error(this->pos_, "integer required");
    }
    break;
  }
  case absyn::Oper::LT_OP:
  case absyn::Oper::LE_OP:
  case absyn::Oper::GT_OP:
  case absyn::Oper::GE_OP:
  case absyn::Oper::EQ_OP:
  case absyn::Oper::NEQ_OP:
  default: {
    if (!leftTy->IsSameType(rightTy)) {
      errormsg->Error(this->pos_, "same type required");
    }
    break;
  }
  }
  return type::IntTy::Instance();
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = tenv->Look(this->typ_);
  if (!ty) {
    errormsg->Error(this->pos_, "undefined type %s",
                    this->typ_->Name().c_str());
    return type::IntTy::Instance();
  }

  type::RecordTy *recordTy = dynamic_cast<type::RecordTy *>(ty->ActualTy());
  if (!recordTy) {
    errormsg->Error(this->pos_, "%s is not a record type",
                    this->typ_->Name().c_str());
    return type::IntTy::Instance();
  }

  const auto &thisFieldsList = this->fields_->GetList();
  const auto &tyFieldsList = recordTy->fields_->GetList();

  auto thisIter = thisFieldsList.cbegin();
  auto tyIter = tyFieldsList.cbegin();

  while (thisIter != thisFieldsList.cend() && tyIter != tyFieldsList.cend()) {
    if ((*thisIter)->name_ != (*tyIter)->name_) {
      errormsg->Error(
          this->pos_, "record field name mismatch, expected %s, got %s",
          (*tyIter)->name_->Name().c_str(), (*thisIter)->name_->Name().c_str());
      return type::IntTy::Instance();
    }
    type::Ty *fieldTy =
        (*thisIter)->exp_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!(*tyIter)->ty_->IsSameType(fieldTy)) {
      errormsg->Error(this->pos_, "record field type mismatch");
      return type::IntTy::Instance();
    }
    thisIter++;
    tyIter++;
  }

  if (thisFieldsList.size() != tyFieldsList.size()) {
    errormsg->Error(this->pos_, "record field number mismatch");
    return type::IntTy::Instance();
  }

  return ty;
}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  const auto &seqList = this->seq_->GetList();
  if (seqList.empty())
    return type::VoidTy::Instance();
  type::Ty *lastTy;

  for (auto iter = seqList.cbegin(); iter != seqList.cend(); iter++) {
    lastTy = (*iter)->SemAnalyze(venv, tenv, labelcount, errormsg);
  }

  return lastTy;
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  absyn::SimpleVar *simpleVar = dynamic_cast<absyn::SimpleVar *>(this->var_);
  if (simpleVar) {
    env::EnvEntry *entry = venv->Look(simpleVar->sym_);
    if (entry->readonly_) {
      errormsg->Error(this->pos_, "loop variable can't be assigned");
    }
  }

  type::Ty *varTy = this->var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *expTy = this->exp_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (!varTy->IsSameType(expTy)) {
    errormsg->Error(this->pos_, "unmatched assign exp");
  }

  return type::VoidTy::Instance();
}

type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *testTy = this->test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *thenTy = this->then_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (!testTy->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(this->pos_, "test statement in if exp is not integer");
  }

  if (!this->elsee_) {
    if (!thenTy->IsSameType(type::VoidTy::Instance())) {
      errormsg->Error(this->pos_, "if-then exp's body must produce no value");
    }
    return type::VoidTy::Instance();
  } else {
    type::Ty *elseTy =
        this->elsee_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!thenTy->IsSameType(elseTy)) {
      errormsg->Error(this->pos_, "then exp and else exp type mismatch");
    }
    return thenTy->ActualTy();
  }
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *testTy = this->test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!testTy->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(this->pos_, "test statement in while exp is not integer");
  }

  if (this->body_) {
    type::Ty *bodyTy =
        this->body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg);
    if (!bodyTy->IsSameType(type::VoidTy::Instance())) {
      errormsg->Error(this->pos_, "while body must produce no value");
    }
  }

  return type::VoidTy::Instance();
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *loTy = this->lo_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *hiTy = this->hi_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (!loTy->IsSameType(type::IntTy::Instance()) ||
      !hiTy->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(this->pos_, "for exp's range type is not integer");
  }

  if (this->body_) {
    venv->BeginScope();
    venv->Enter(var_, new env::VarEntry(type::IntTy::Instance(), true));
    type::Ty *bodyTy =
        this->body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg);
    venv->EndScope();
    if (!bodyTy->IsSameType(type::VoidTy::Instance())) {
      errormsg->Error(this->pos_, "for exp's body must produce no value");
    }
  }

  return type::VoidTy::Instance();
}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  if (labelcount == 0) {
    errormsg->Error(this->pos_, "break is not inside any loop");
  }
  return type::VoidTy::Instance();
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  venv->BeginScope();
  tenv->BeginScope();

  const auto &decList = this->decs_->GetList();
  for (auto iter = decList.cbegin(); iter != decList.cend(); iter++) {
    (*iter)->SemAnalyze(venv, tenv, labelcount, errormsg);
  }

  type::Ty *res = type::VoidTy::Instance();

  if (this->body_) {
    res = this->body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  }

  venv->EndScope();
  tenv->EndScope();

  return res;
}

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  auto entry = tenv->Look(this->typ_);
  if (!entry) {
    errormsg->Error(this->pos_, "undefined type %s",
                    this->typ_->Name().c_str());
    return type::IntTy::Instance();
  }
  type::ArrayTy *arrayTy = dynamic_cast<type::ArrayTy *>(entry->ActualTy());
  if (!arrayTy) {
    errormsg->Error(this->pos_, "array type required");
    return type::IntTy::Instance();
  }
  type::Ty *sizeTy = this->size_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!sizeTy->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(this->pos_, "array size must be int");
  }
  type::Ty *initTy = this->init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!initTy->IsSameType(arrayTy->ty_)) {
    errormsg->Error(this->pos_, "type mismatch");
  }
  // errormsg->Error(this->pos_,"%s",typeid(arrayTy).name());//TODO: remove this
  return arrayTy;
}

type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  const auto &functionList = this->functions_->GetList();
  for (auto iter = functionList.cbegin(); iter != functionList.cend(); iter++) {
    FunDec *funDec = *iter;
    if (venv->Look(funDec->name_)) {
      errormsg->Error(this->pos_, "two functions have the same name");
    }
    type::TyList *formalTyList =
        funDec->params_->MakeFormalTyList(tenv, errormsg);
    if (funDec->result_) {
      type::Ty *resultTy = tenv->Look(funDec->result_);
      if (!resultTy) {
        errormsg->Error(this->pos_, "undefined type %s",
                        funDec->result_->Name().c_str());
      }
      venv->Enter(funDec->name_, new env::FunEntry(formalTyList, resultTy));
    } else {
      venv->Enter(funDec->name_,
                  new env::FunEntry(formalTyList, type::VoidTy::Instance()));
    }
  }

  for (auto iter = functionList.cbegin(); iter != functionList.cend(); iter++) {
    FunDec *funDec = *iter;
    type::TyList *formalTyList =
        funDec->params_->MakeFormalTyList(tenv, errormsg);

    venv->BeginScope();

    const auto &paramsList = funDec->params_->GetList();
    const auto &tyList = formalTyList->GetList();

    auto tyIter = tyList.cbegin();
    auto paramsIter = paramsList.cbegin();
    while (paramsIter != paramsList.cend()) {
      venv->Enter((*paramsIter)->name_, new env::VarEntry(*tyIter));
      tyIter++;
      paramsIter++;
    }

    type::Ty *bodyTy =
        funDec->body_->SemAnalyze(venv, tenv, labelcount, errormsg);
    env::EnvEntry *entry = venv->Look(funDec->name_);
    env::FunEntry *funEntry = dynamic_cast<env::FunEntry *>(entry);
    if (!funEntry) {
      errormsg->Error(this->pos_, "invalid function type");
    }
    if (!bodyTy->IsSameType(funEntry->result_)) {
      if (funEntry->result_->IsSameType(type::VoidTy::Instance())) {
        errormsg->Error(this->pos_, "procedure returns value");
      } else {
        errormsg->Error(this->pos_, "function return type mismatch");
      }
    }
    venv->EndScope();
  }
}

void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *initTy = this->init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!this->typ_) {
    if (dynamic_cast<type::NilTy *>(initTy->ActualTy())) {
      errormsg->Error(this->pos_,
                      "init should not be nil without type specified");
      initTy = type::IntTy::Instance(); // TODO: check this
    }
    venv->Enter(this->var_, new env::VarEntry(initTy));
  } else {
    type::Ty *ty = tenv->Look(this->typ_);
    if (!ty) {
      errormsg->Error(this->pos_, "undefined type %s",
                      this->typ_->Name().c_str());
    }
    if (!initTy->IsSameType(ty)) {
      errormsg->Error(this->pos_, "type mismatch");
    }
    venv->Enter(this->var_, new env::VarEntry(ty));
  }
}

void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  const auto &nameAnaTyList = this->types_->GetList();
  for (auto iter = nameAnaTyList.cbegin(); iter != nameAnaTyList.cend();
       iter++) {
    auto nameAndTy = *iter;
    if (tenv->Look(nameAndTy->name_)) {
      errormsg->Error(this->pos_, "two types have the same name");
    } else {
      tenv->Enter(nameAndTy->name_,
                  new type::NameTy(nameAndTy->name_, nullptr));
    }
  }

  for (auto iter = nameAnaTyList.cbegin(); iter != nameAnaTyList.cend();
       iter++) {
    auto nameAndTy = *iter;
    type::Ty *ty = tenv->Look(nameAndTy->name_);
    type::NameTy *nameTy = dynamic_cast<type::NameTy *>(ty);
    assert(nameTy); // should find and be NameTy
    nameTy->ty_ = nameAndTy->ty_->SemAnalyze(tenv, errormsg);
  }

  bool cycle = false;
  for (auto iter = nameAnaTyList.cbegin(); iter != nameAnaTyList.cend();
       iter++) {
    type::Ty *startTy = tenv->Look((*iter)->name_);
    type::NameTy *startNameTy;
    if (startNameTy = dynamic_cast<type::NameTy *>(startTy)) {
      type::Ty *currentTy = startNameTy->ty_;
      type::NameTy *nameTy;

      while (nameTy = dynamic_cast<type::NameTy *>(currentTy)) {
        if (nameTy->sym_ == startNameTy->sym_) {
          errormsg->Error(this->pos_, "illegal type cycle");
          cycle = true;
          break;
        }
        currentTy = nameTy->ty_;
      }
    }
    if (cycle)
      break;
  }
}

type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = tenv->Look(this->name_);
  if (!ty) {
    errormsg->Error(this->pos_, "undefined type %s",
                    this->name_->Name().c_str());
    return type::IntTy::Instance();
  }
  return new type::NameTy(this->name_, ty);
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return new type::RecordTy(this->record_->MakeFieldList(tenv, errormsg));
}

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = tenv->Look(this->array_);
  if (!ty) {
    errormsg->Error(this->pos_, "undefined type %s",
                    this->array_->Name().c_str());
    return new type::ArrayTy(type::IntTy::Instance());
  } else {
    return new type::ArrayTy(ty);
  }
}

} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}
} // namespace sem
