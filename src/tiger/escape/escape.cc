#include "tiger/escape/escape.h"
#include "tiger/absyn/absyn.h"

namespace esc {
void EscFinder::FindEscape() { absyn_tree_->Traverse(env_.get()); }
} // namespace esc

namespace absyn {

void AbsynTree::Traverse(esc::EscEnvPtr env) {
  /* TODO: Put your lab5 code here */
  this->root_->Traverse(env,0);
}

void SimpleVar::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  auto *entry=env->Look(this->sym_);
  if(entry!=nullptr&&depth>entry->depth_){
    *(entry->escape_)=true;
  }
}

void FieldVar::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  this->var_->Traverse(env,depth);
  auto *entry=env->Look(this->sym_);
  if(entry!=nullptr&&depth>entry->depth_){
    *(entry->escape_)=true;
  }
}

void SubscriptVar::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  this->var_->Traverse(env,depth);
}

void VarExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  this->var_->Traverse(env,depth);
}

void NilExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
}

void IntExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
}

void StringExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
}

void CallExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  const auto &list=this->args_->GetList();
  for(const auto &arg:list){
    arg->Traverse(env,depth);
  }
}

void OpExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  this->left_->Traverse(env,depth);
  this->right_->Traverse(env,depth);
}

void RecordExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  const auto &list=this->fields_->GetList();
  for (const auto &item:list){
    item->exp_->Traverse(env,depth);
  }
}

void SeqExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  const auto &list=this->seq_->GetList();
  for (const auto &exp:list){
    exp->Traverse(env,depth);
  }
}

void AssignExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  this->var_->Traverse(env,depth);
  this->exp_->Traverse(env,depth);
}

void IfExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  this->test_->Traverse(env,depth);
  this->then_->Traverse(env,depth);
  if(this->elsee_!=nullptr){
    this->elsee_->Traverse(env,depth);
  }
}

void WhileExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  this->test_->Traverse(env,depth);
  this->body_->Traverse(env,depth);
}

void ForExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  if(this->body_==nullptr)return;
  env->BeginScope();
  this->escape_=false;
  env->Enter(this->var_,new esc::EscapeEntry(depth,&this->escape_));
  this->lo_->Traverse(env,depth);
  this->hi_->Traverse(env,depth);
  this->body_->Traverse(env,depth);
  env->EndScope();
}

void BreakExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
}

void LetExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  env->BeginScope();
  const auto &list=this->decs_->GetList();
  for(const auto &dec:list){
    dec->Traverse(env,depth);
  }
  this->body_->Traverse(env,depth);
  env->EndScope();
}

void ArrayExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  this->size_->Traverse(env,depth);
  this->init_->Traverse(env,depth);
}

void VoidExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
}

void FunctionDec::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  const auto &list=this->functions_->GetList();
  for(const auto &dec:list){
    if(dec->body_==nullptr)continue;
    env->BeginScope();
    const auto &params_list=dec->params_->GetList();
    for(const auto &param:params_list){
      param->escape_=false;
      env->Enter(param->name_,new esc::EscapeEntry(depth+1,&param->escape_));
    }
    dec->body_->Traverse(env,depth+1);
    env->EndScope();
  }
}

void VarDec::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  this->escape_=false;
  env->Enter(this->var_,new esc::EscapeEntry(depth,&this->escape_));
  this->init_->Traverse(env,depth);
}

void TypeDec::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
}

} // namespace absyn
