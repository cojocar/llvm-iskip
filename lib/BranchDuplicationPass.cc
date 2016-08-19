// Copyright 2016 The Iskip Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#define DEBUG_TYPE "iskip-branch-dup"

#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/CodeGen/MachineInstrBuilder.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetInstrInfo.h>
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Pass.h"
#include "llvm/MC/MCInst.h"
#include "llvm/ADT/Statistic.h"
#include "llvm-src/include/llvm/CodeGen/MachineConstantPool.h"
#include "llvm-src/lib/Target/ARM/ARMMachineFunctionInfo.h"
#include "llvm-src/lib/Target/ARM/ARMConstantPoolValue.h"

#include "BranchDuplicationPass.h"
#include "util.h"

STATISTIC(BranchesDuplicated, "The # of branches duplicated");
STATISTIC(BranchesNotDuplicated, "The # of branches not duplicated");

using namespace llvm;

cl::opt<bool>
llvm::IskipEnableBranchDuplication("iskip-enable-branch-duplication",
                              cl::NotHidden,
                 cl::desc("Enable Branch Duplication Pass"),
                 cl::init(false));


namespace gen_inc {
#define __GET_TB
#include "tB.inc"
#define __GET_T2B
#include "t2B.inc"
}

namespace {
char BranchDuplicationPass::ID = 0;

bool BranchDuplicationPass::runOnMachineFunction(MachineFunction &Fn)
{
  bool modified = false;

  if (!ARMHardeningUtil::shouldRunPassOnFunction(Fn))
    return false;

  STI = &static_cast<const ARMSubtarget &>(Fn.getSubtarget());
  TII = STI->getInstrInfo();

  if (ARMHardeningUtil::IskipEnableVerbose)
    errs() << "ARMHardening(external, branch duplication): " <<
      Fn.getName() << "\n";
  for (auto &bb : Fn) {
    modified |= branchDuplicationBB(bb);
  }

  if (ARMHardeningUtil::IskipEnableVerbose)
    errs() << BranchDuplicationPass::MyBranchesDuplicated <<
      " branches duplicated" << "\n";
  return modified;
}

bool BranchDuplicationPass::branchDuplication(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator &MBBI)
{
  if (!STI->isThumb2())
    return false;

  switch (MBBI->getDesc().getOpcode()) {
  default: return false;
  case ARM::t2B:
  case ARM::t2Bcc:
           break;
  }

  ++BranchesDuplicated;
  ++BranchDuplicationPass::MyBranchesDuplicated;

  DebugLoc DL = MBBI->getDebugLoc();

  //errs() << "Attempt duplication of: " << *MBBI << "\n";
  assert(true == MBBI->isBranch());
  // TODO: assert instruction is idempotent

  // search for target basic block and for the condition flag
  // XXX: we should use the position of the operands
  MachineBasicBlock *targetBB = nullptr;
  int64_t cond = (int64_t)ARMCC::AL;
  for (MachineInstr::mop_iterator MO = MBBI->operands_begin();
       MO != MBBI->operands_end();
       ++MO) {
    //errs() << "OP:" << *MO << "\n";
    if (MO->isMBB()) {
      targetBB = MO->getMBB();
    }
    if (MO->isImm())
      cond = MO->getImm();
  }

  assert(targetBB != nullptr && "Target BB not found when duplicating instructions");

  unsigned opcode = MBBI->getOpcode();
  // this inserts before the iterator
  BuildMI(MBB, MBBI, DL, TII->get(opcode))
    .addMBB(targetBB)
    .addImm(cond)
    .addReg(0);
  // on O2 the unconditional branch duplication is removed by the
  // 'analyzeBranch' function

  return true;
}

bool BranchDuplicationPass::branchDuplicationBB(MachineBasicBlock &MBB)
{
  MachineBasicBlock::iterator MBBI;
  bool modified = false;

  if (MBB.begin() == MBB.end())
    return false;

  for (MBBI = MBB.begin(); MBBI != MBB.end(); ++MBBI) {
    //errs() << "INSN: " << *MBBI << "\n";
    if (isBranch(MBBI) && !ARMHardeningUtil::isInsideITBundle(MBBI)) {
      bool m = branchDuplication(MBB, MBBI);
      BranchesNotDuplicated += !m;
      modified |= m;
      if (m) {
      } else {
        errs() << "No duplication for: " << *MBBI << "\n";
      }
    } else {
        //errs() << "Not a branch: " << *MBBI << "\n";
    }
  }
  return modified;
}

bool BranchDuplicationPass::isBranch(MachineBasicBlock::iterator &MBBI)
{
  const MCInstrDesc &desc = MBBI->getDesc();

#define check_in_array(a, opcode)                \
  do {                                           \
    unsigned idx;                                \
    for (idx = 0; idx < ARRAY_LEN(a); ++idx) {   \
      if (a[idx] == opcode)                      \
      return true;                               \
    }                                            \
  } while (0)

  check_in_array(gen_inc::__insn_tb, desc.getOpcode());
  check_in_array(gen_inc::__insn_t2b, desc.getOpcode());

  return false;
}

}

FunctionPass *llvm::createBranchDuplicationPass() {
  return new BranchDuplicationPass();
}
