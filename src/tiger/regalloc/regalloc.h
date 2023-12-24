#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include "tiger/codegen/assem.h"
#include "tiger/codegen/codegen.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/regalloc/color.h"
#include "tiger/util/graph.h"

namespace ra {

class Result {
public:
  temp::Map *coloring_;
  assem::InstrList *il_;

  Result() : coloring_(nullptr), il_(nullptr) {}
  Result(temp::Map *coloring, assem::InstrList *il)
      : coloring_(coloring), il_(il) {}
  Result(const Result &result) = delete;
  Result(Result &&result) = delete;
  Result &operator=(const Result &result) = delete;
  Result &operator=(Result &&result) = delete;
  ~Result() {
    delete coloring_;
    delete il_;
  }
};

class RegAllocator {
  /* TODO: Put your lab6 code here */
public:
  RegAllocator(frame::Frame *frame, std::unique_ptr<cg::AssemInstr> assem_instr)
      : frame_(frame), assem_instr_(std::move(assem_instr)) {
    instr_list_ = assem_instr_->GetInstrList();
  }
  void RegAlloc();
  col::Result AllocateOneRound();
  std::unique_ptr<Result> TransferResult() {
    return std::make_unique<Result>(temp_map_, instr_list_);
  }

private:
  frame::Frame *frame_;
  std::unique_ptr<cg::AssemInstr> assem_instr_;
  assem::InstrList *instr_list_;
  temp::Map *temp_map_;

  void rewrite_program(const std::list<temp::Temp *> &spills);
  void remove_trivial_moves();
};

} // namespace ra

#endif