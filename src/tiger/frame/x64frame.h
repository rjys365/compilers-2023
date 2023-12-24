//
// Created by wzl on 2021/10/12.
//

#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

namespace frame {

class X64RegManager : public RegManager {
  /* TODO: Put your lab5 code here */
private:
  std::vector<temp::Temp *> registers;
  void add_register(std::string name);

public:
  X64RegManager();
  ~X64RegManager();

  temp::TempList *Registers() override;
  temp::TempList *ArgRegs() override;
  temp::TempList *CallerSaves() override;
  temp::TempList *CalleeSaves() override;
  temp::TempList *ReturnSink() override;
  int WordSize() override;
  temp::Temp *FramePointer() override;
  temp::Temp *StackPointer() override;
  temp::Temp *ReturnValue() override;
  temp::TempList *SpecialArithmaticOpRegs() override;
};

class X64Frame : public Frame {
  /* TODO: Put your lab5 code here */
private:
  temp::Label *label;
  std::list<Access *> formal_list;
  int last_frame_var_offset = 0;
  std::list<std::pair<temp::Temp *, temp::Temp *>> saved_regs_;

public:
  // this class should be able to be created by
  // Constructor(temp::Label *name, std::list<bool> formals)
  X64Frame(temp::Label *name, const std::list<bool> &formals);
  std::string getlabel() override;

  Access *allocLocal(bool escape) override;

  std::list<Access *> &formals() override;

  temp::Label *name() override;

  tree::Exp *externalCall(std::string s, tree::ExpList *args) override;

  tree::Stm *procEntryExit1(tree::Stm *stm) override;

  assem::InstrList *procEntryExit2(assem::InstrList *body);

  assem::Proc *procEntryExit3(assem::InstrList *body);

  int directlyAllocInFrameLocal() override;
};

} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
