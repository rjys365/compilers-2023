#ifndef TIGER_CODEGEN_CODEGEN_H_
#define TIGER_CODEGEN_CODEGEN_H_

#include "tiger/canon/canon.h"
#include "tiger/codegen/assem.h"
#include "tiger/frame/x64frame.h"
#include "tiger/translate/tree.h"

// Forward Declarations
namespace frame {
class RegManager;
class Frame;
} // namespace frame

namespace assem {
class Instr;
class InstrList;
} // namespace assem

namespace canon {
class Traces;
} // namespace canon

namespace cg {

enum class MemoryAddressingMode {
  INVALID = 0,
  DISPLACEMENT = 1, // normal is displacement with 0 displacement
  INDEX = 2
};

struct MemoryAddress {
  MemoryAddressingMode mode = MemoryAddressingMode::INVALID;
  int64_t value;
  tree::Exp *base;
  int8_t scale;
  tree::Exp *index;

  std::string
  munch(assem::InstrList &instr_list, std::string_view fs, int &current_temp_idx, temp::TempList *temp_list);
};

// TODO: munch memory address
// now not using them and only process basic mov instruction

class AssemInstr {
public:
  AssemInstr() = delete;
  AssemInstr(nullptr_t) = delete;
  explicit AssemInstr(assem::InstrList *instr_list) : instr_list_(instr_list) {}

  void Print(FILE *out, temp::Map *map) const;

  [[nodiscard]] assem::InstrList *GetInstrList() const { return instr_list_; }

private:
  assem::InstrList *instr_list_;
};

class CodeGen {
public:
  CodeGen(frame::Frame *frame, std::unique_ptr<canon::Traces> traces)
      : frame_(frame), traces_(std::move(traces)) {}

  void Codegen();
  std::unique_ptr<AssemInstr> TransferAssemInstr() {
    return std::move(assem_instr_);
  }

private:
  frame::Frame *frame_;
  std::string fs_; // Frame size label_
  std::unique_ptr<canon::Traces> traces_;
  std::unique_ptr<AssemInstr> assem_instr_;
  std::list<std::pair<temp::Temp *, temp::Temp *>> saved_regs_;
};

} // namespace cg
#endif
