#include "straightline/slp.h"

#include <iostream>

namespace A {
int max(int a, int b) { return a > b ? a : b; }

int A::CompoundStm::MaxArgs() const {
  return max(this->stm1->MaxArgs(), this->stm2->MaxArgs());
}

Table *A::CompoundStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  Table * result1=this->stm1->Interp(t);
  return this->stm2->Interp(result1);
}

int A::AssignStm::MaxArgs() const { return this->exp->maxArgs(); }

Table *A::AssignStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  IntAndTable *result=this->exp->interp(t);
  return result->t->Update(this->id,result->i);
}

int A::PrintStm::MaxArgs() const { return this->exps->maxArgs(); }

Table *A::PrintStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  ExpList *list=this->exps;
  Table *table=t;
  while(true){
    PairExpList *pairExpList=dynamic_cast<PairExpList *>(list);
    if(!pairExpList)break;
    IntAndTable *result= pairExpList->getExp()->interp(table);
    table=result->t;
    std::cout<<result->i<<" ";
    list=pairExpList->getTail();
  }
  IntAndTable *result= list->getExp()->interp(table);
  table=result->t;
  std::cout<<result->i<<std::endl;
  return table;
}

Exp *PairExpList::getExp() const { return this->exp; }

ExpList *PairExpList::getTail() const { return this->tail; }

int PairExpList::maxArgs() const {
  return max(1 + this->tail->maxArgs(), this->exp->maxArgs());
}

Exp *LastExpList::getExp() const { return this->exp; }

int LastExpList::maxArgs() const { return max(1, this->exp->maxArgs()); }

int Exp::maxArgs() const {
  return 0;
}

int EseqExp::maxArgs() const {
  return max(this->stm->MaxArgs(),this->exp->maxArgs());
}

IntAndTable *IdExp::interp(Table * table)const{
  return new IntAndTable(table->Lookup(this->id),table);
}

IntAndTable *NumExp::interp(Table *table)const{
  return new IntAndTable(this->num,table);
}

IntAndTable *OpExp::interp(Table *table)const{
  IntAndTable *leftResult=this->left->interp(table);
  IntAndTable *rightResult=this->right->interp(leftResult->t);
  switch(this->oper){
    case PLUS:
      return new IntAndTable(leftResult->i+rightResult->i,rightResult->t);
    case MINUS:
      return new IntAndTable(leftResult->i-rightResult->i,rightResult->t);
    case TIMES:
      return new IntAndTable(leftResult->i*rightResult->i,rightResult->t);
    case DIV:
      return new IntAndTable(leftResult->i/rightResult->i,rightResult->t);
  }
}

IntAndTable *EseqExp::interp(Table *table)const{
  Table *stmResult=this->stm->Interp(table);
  return this->exp->interp(stmResult);
}

int Table::Lookup(const std::string &key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(const std::string &key, int val) const {
  return new Table(key, val, this);
}
} // namespace A
