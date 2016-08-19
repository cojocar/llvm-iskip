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

#define DEBUG_TYPE "iskip-dup"

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

#include "InstructionDuplicationPass.h"
#include "util.h"

STATISTIC(InstructionDuplicated, "The # of instruction OK duplicated");
STATISTIC(InstructionNotDuplicated, \
          "The # of instructions NOT duplicated (attempts)");
STATISTIC(InstructionNotDuplicatedTotal,
          "The # of instructions NOT duplicated (all), i.e. not idempotent");
STATISTIC(InstructionTotal,
          "The total # of instructions");

using namespace llvm;
cl::opt<bool>
llvm::IskipEnableInstructionDuplication("iskip-enable-instruction-duplication",
                              cl::NotHidden,
                 cl::desc("Enable Instruction Duplication Pass"),
                 cl::init(false));

namespace {
char InstructionDuplicationPass::ID = 0;
bool InstructionDuplicationPass::duplicateInstruction(MachineBasicBlock &MBB,
                                           MachineBasicBlock::iterator
                                           &MBBI)
{

  if (ARMHardeningUtil::isInsideITBundle(MBBI))
    return false;

  //if (MBBI->isTerminator())
  //  return false;

  if (MBBI->isNotDuplicable())
    return false;

  MachineFunction *MF = MBB.getParent();

  for (unsigned cnt = 0; cnt < DuplicationFactor; ++cnt) {
    MachineInstr *clone = MF->CloneMachineInstr(&(*MBBI));
    MBB.insert(MBBI, clone);
  }

  return true;
}

bool InstructionDuplicationPass::duplicateInstructionBB(MachineBasicBlock &MBB)
{
  MachineBasicBlock::iterator MBBI;
  bool modified = false;

  if (MBB.begin() == MBB.end())
    return false;

  for (MBBI = MBB.begin(); MBBI != MBB.end(); ++MBBI) {
    if (ARMHardeningUtil::isInsnIdempotent(MBBI)) {
      bool m = duplicateInstruction(MBB, MBBI);
      InstructionNotDuplicated += !m;
      InstructionDuplicated += !!m;
      modified |= m;
      if (m) {
        ++InstructionDuplicationPass::MyInstructionDuplicated;
        //break;
      } else {
        errs() << "No duplication for: " << *MBBI << "\n";
      }
    } else {
      ++InstructionNotDuplicatedTotal;
    }
    ++InstructionTotal;
  }
  return modified;
}

bool InstructionDuplicationPass::runOnMachineFunction(MachineFunction &Fn)
{
  bool modified = false;

  if (!ARMHardeningUtil::shouldRunPassOnFunction(Fn))
    return false;

  STI = &static_cast<const ARMSubtarget &>(Fn.getSubtarget());
  TII = STI->getInstrInfo();

  errs() << "ARMHardening(external, instruction duplication): " <<
    Fn.getName() << "\n";
  for (auto &bb : Fn) {
    modified |= duplicateInstructionBB(bb);
  }

  errs() << InstructionDuplicationPass::MyInstructionDuplicated <<
    " instructions duplicated" << "\n";
  return modified;
}

}

FunctionPass *llvm::createInstructionDuplicationPass() {
  return new InstructionDuplicationPass();
}
