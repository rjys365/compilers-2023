//
// Created by wzl on 2021/10/12.
//

#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

namespace frame {
class X64RegManager : public RegManager {
  /* TODO: Put your lab5 code here */
  // TODO: fill this
  temp::TempList *Registers(){return new temp::TempList();}
  temp::TempList *ArgRegs(){return new temp::TempList();}
  temp::TempList *CallerSaves(){return new temp::TempList();}
  temp::TempList *CalleeSaves(){return new temp::TempList();}
  temp::TempList *ReturnSink(){return new temp::TempList();}
  virtual int WordSize(){return 8;}
  temp::Temp *FramePointer(){return temp::TempFactory::NewTemp();}
  temp::Temp *StackPointer(){return temp::TempFactory::NewTemp();}
  temp::Temp *ReturnValue(){return temp::TempFactory::NewTemp();}
  
};

} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
