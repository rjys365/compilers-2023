#include "tiger/frame/x64frame.h"

extern frame::RegManager *reg_manager;

namespace frame {
/* TODO: Put your lab5 code here */
class InFrameAccess : public Access {
public:
  int offset;

  explicit InFrameAccess(int offset) : offset(offset) {}
  /* TODO: Put your lab5 code here */
  tree::Exp *Exp(tree::Exp *framePtr) override {
    return new tree::MemExp(new tree::BinopExp(tree::BinOp::PLUS_OP, framePtr,
                                               new tree::ConstExp(offset)));
  }
};

class InRegAccess : public Access {
public:
  temp::Temp *reg;

  explicit InRegAccess(temp::Temp *reg) : reg(reg) {}
  /* TODO: Put your lab5 code here */
  tree::Exp *Exp(tree::Exp *framePtr) override {
    return new tree::TempExp(reg);
  }
};

X64Frame::X64Frame(temp::Label *name, const std::list<bool> &formals)
    : label(name) {
  for (auto &formal : formals) {
    Access *access = allocLocal(formal);
    formal_list.push_back(access);
  }
}

std::string X64Frame::getlabel() {
  return temp::LabelFactory::LabelString(label);
}

Access *X64Frame::allocLocal(bool escape) {
  if (escape) {
    last_frame_var_offset -= reg_manager->WordSize();
    frame::InFrameAccess *access = new InFrameAccess(last_frame_var_offset);
    return access;
  } else {
    frame::InRegAccess *access = new InRegAccess(temp::TempFactory::NewTemp());
    return access;
  }
}

std::list<Access *> &X64Frame::formals() { return formal_list; }

temp::Label *X64Frame::name() { return label; }

tree::Exp *X64Frame::externalCall(std::string s, tree::ExpList *args) {
  return new tree::CallExp(new tree::NameExp(temp::LabelFactory::NamedLabel(s)),
                           args);
}

tree::Stm *X64Frame::procEntryExit1(tree::Stm *stm) {
  // TODO

  // view shift: copy args to registers and stack
  auto &arg_regs_list = reg_manager->ArgRegs()->GetList();
  auto arg_regs_iter = arg_regs_list.begin();
  int arg_cnt = 0;

  tree::Stm *save_stm = nullptr;
  auto &callee_saves_list = reg_manager->CalleeSaves()->GetList();
  for (auto &callee_save : callee_saves_list) {
    temp::Temp *saved_temp = temp::TempFactory::NewTemp();
    saved_regs_.emplace_back(callee_save, saved_temp);
    if (save_stm == nullptr) {
      save_stm = new tree::MoveStm(new tree::TempExp(saved_temp),
                                   new tree::TempExp(callee_save));
    } else {
      save_stm = new tree::SeqStm(
          save_stm, new tree::MoveStm(new tree::TempExp(saved_temp),
                                      new tree::TempExp(callee_save)));
    }
  }

  tree::Stm *restore_stm = nullptr;
  for (auto iter = saved_regs_.rbegin(); iter != saved_regs_.rend(); iter++) {
    if (restore_stm == nullptr) {
      restore_stm = new tree::MoveStm(new tree::TempExp(iter->first),
                                      new tree::TempExp(iter->second));
    } else {
      restore_stm = new tree::SeqStm(
          restore_stm, new tree::MoveStm(new tree::TempExp(iter->first),
                                         new tree::TempExp(iter->second)));
    }
  }

  tree::Stm *new_stm = nullptr;

  for (auto &formal : formal_list) {
    tree::Exp *formal_exp = nullptr;
    if (arg_regs_iter != arg_regs_list.end()) {
      formal_exp = new tree::TempExp(*arg_regs_iter);
    } else {
      formal_exp = new tree::MemExp(new tree::BinopExp(
          tree::BinOp::PLUS_OP, new tree::TempExp(reg_manager->FramePointer()),
          new tree::ConstExp((arg_cnt - 6 + 1) * reg_manager->WordSize())));
      // no pushq %rbp, so only need to skip the return value
    }
    tree::Stm *move_stm = new tree::MoveStm(
        formal->Exp(new tree::TempExp(reg_manager->FramePointer())),
        formal_exp);
    if (new_stm == nullptr) {
      new_stm = move_stm;
    } else {
      new_stm = new tree::SeqStm(new_stm, move_stm);
    }
    arg_cnt++;
    if (arg_regs_iter != arg_regs_list.end())
      arg_regs_iter++;
  }
  if (new_stm == nullptr) {
    new_stm = stm;
  } else {
    new_stm = new tree::SeqStm(new_stm, stm);
  }
  new_stm = new tree::SeqStm(save_stm, new tree::SeqStm(new_stm, restore_stm));
  return new_stm;
}

assem::InstrList *X64Frame::procEntryExit2(assem::InstrList *body) {
  body->Append(new assem::OperInstr("", new temp::TempList(),
                                    reg_manager->ReturnSink(), nullptr));
  return body;
}

assem::Proc *X64Frame::procEntryExit3(assem::InstrList *body) {
  // TODO
  std::string prolog = ".set " + this->label->Name() + "_framesize, " +
                       std::to_string(-last_frame_var_offset) + "\n";
  prolog += this->label->Name() + ":\n";

  prolog += "subq $" + std::to_string(-last_frame_var_offset) + ", %rsp\n";

  std::string epilog =
      "addq $" + std::to_string(-last_frame_var_offset) + ", %rsp\n";
  epilog += "retq\n";

  return new assem::Proc(prolog, body, epilog);
}

int X64Frame::directlyAllocInFrameLocal() {
  last_frame_var_offset -= reg_manager->WordSize();
  return last_frame_var_offset;
}

/* TODO: Put your lab5 code here */

void X64RegManager::add_register(std::string name) {
  temp::Temp *new_temp = temp::TempFactory::NewTemp();
  registers.push_back(new_temp);
  temp_map_->Enter(new_temp, new std::string(name));
}

// comments are hints generated by GPT
X64RegManager::X64RegManager() {
  // Initialize X64RegManager specific members if needed.
  add_register("%rax");
  add_register("%rbx");
  add_register("%rcx");
  add_register("%rdx");
  add_register("%rsi");
  add_register("%rdi");
  add_register("%rbp");
  add_register("%rsp");
  add_register("%r8");
  add_register("%r9");
  add_register("%r10");
  add_register("%r11");
  add_register("%r12");
  add_register("%r13");
  add_register("%r14");
  add_register("%r15");
  add_register("virtual_frame_pointer"); // shouldn't appear in real assembly
}

X64RegManager::~X64RegManager() {
  // Clean up if needed.
}

temp::TempList *X64RegManager::Registers() {
  // Implement the logic to return general-purpose registers, excluding RSP.
  temp::TempList *temp_list = new temp::TempList();
  for (int i = 0; i < 16; i++) {
    if (i != 7) {
      temp_list->Append(registers[i]);
    }
  }
  return temp_list;
}

temp::TempList *X64RegManager::ArgRegs() {
  // Implement the logic to return argument registers.
  temp::TempList *temp_list = new temp::TempList();
  temp_list->Append(registers[5]); // rdi
  temp_list->Append(registers[4]); // rsi
  temp_list->Append(registers[3]); // rdx
  temp_list->Append(registers[2]); // rcx
  temp_list->Append(registers[8]); // r8
  temp_list->Append(registers[9]); // r9
  return temp_list;
}

temp::TempList *X64RegManager::CallerSaves() {
  // Implement the logic to return caller-saved registers.
  temp::TempList *temp_list = new temp::TempList();
  temp_list->Append(registers[0]);  // rax
  temp_list->Append(registers[5]);  // rdi
  temp_list->Append(registers[4]);  // rsi
  temp_list->Append(registers[3]);  // rdx
  temp_list->Append(registers[2]);  // rcx
  temp_list->Append(registers[8]);  // r8
  temp_list->Append(registers[9]);  // r9
  temp_list->Append(registers[10]); // r10
  temp_list->Append(registers[11]); // r11
  return temp_list;
}

temp::TempList *X64RegManager::CalleeSaves() {
  // Implement the logic to return callee-saved registers.
  // will be saved and restored by ProcEntryExit1
  temp::TempList *temp_list = new temp::TempList();
  temp_list->Append(registers[1]);  // rbx
  temp_list->Append(registers[6]);  // rbp
  temp_list->Append(registers[12]); // r12
  temp_list->Append(registers[13]); // r13
  temp_list->Append(registers[14]); // r14
  temp_list->Append(registers[15]); // r15
  return temp_list;
}

temp::TempList *X64RegManager::ReturnSink() {
  // Implement the logic to return return-sink registers.
  temp::TempList *temp_list = CalleeSaves();
  temp_list->Append(StackPointer());
  temp_list->Append(ReturnValue());

  return nullptr; // Replace with actual return value.
}

int X64RegManager::WordSize() {
  // Implement the logic to return the word size for x64 architecture.
  return 8; // For x64, the word size is 8 bytes.
}

temp::Temp *X64RegManager::FramePointer() {
  // Implement the logic to return the frame pointer register.
  // return registers[6]; // rbp
  return registers[16];
}

temp::Temp *X64RegManager::StackPointer() {
  // Implement the logic to return the stack pointer register.
  return registers[7]; // rsp
}

temp::Temp *X64RegManager::ReturnValue() {
  // Implement the logic to return the register for return value.
  return registers[0]; // rax
}

temp::TempList *X64RegManager::SpecialArithmaticOpRegs() {
  temp::TempList *temp_list = new temp::TempList();
  temp_list->Append(registers[0]); // rax
  temp_list->Append(registers[3]); // rdx
  return temp_list;
}

} // namespace frame
