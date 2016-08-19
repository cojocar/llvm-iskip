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

#define DEBUG_TYPE "iskip-nop"

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

#include "NopInsertionPass.h"
#include "util.h"


STATISTIC(InsertedNops, "The # of nops inserted");

using namespace llvm;

cl::opt<bool>
llvm::IskipEnableNopInsertion("iskip-enable-nop-insertion", cl::NotHidden,
                 cl::desc("Enable Nop Insertion"),
                 cl::init(false));

namespace {
char NopInsertionPass::ID = 0;
bool NopInsertionPass::nopInsert(MachineBasicBlock &MBB,
                                           MachineBasicBlock::iterator
                                           &MBBI)
{
  ++InsertedNops;
  ++NopInsertionPass::MyInsertedNops;

#if 0
  BuildMI(MBB, MBBI, MBBI->getDebugLoc(),
          TII->get(ARM::t2MOVr))
    .addReg(ARM::R0, RegState::Undef) /* Ra */
    .addImm((unsigned)ARMCC::AL)
    .addReg(ARM::R0, RegState::Undef)
    .addReg(0)
    .setMIFlags(0);
#endif
  BuildMI(MBB, MBBI, MBBI->getDebugLoc(),
          TII->get(ARM::tHINT))
    .addImm(0)
    .addImm((unsigned)ARMCC::AL)
    .addReg(0);

  return true;
}

bool NopInsertionPass::nopInsertBB(MachineBasicBlock &MBB)
{
  MachineBasicBlock::iterator MBBI;
  bool modified = false;

  if (MBB.begin() == MBB.end())
    return false;

  for (MBBI = MBB.begin(); MBBI != MBB.end(); ++MBBI) {
    if (ARMHardeningUtil::isInsideITBundle(MBBI))
      continue;
    nopInsert(MBB, MBBI);
    modified = true;
  }

  return modified;
}

bool NopInsertionPass::runOnMachineFunction(MachineFunction &Fn)
{
  bool modified = false;

  if (!ARMHardeningUtil::shouldRunPassOnFunction(Fn))
    return false;

  STI = &static_cast<const ARMSubtarget &>(Fn.getSubtarget());
  TII = STI->getInstrInfo();

  errs() << "ARMHardening(external, nop insertion): " << Fn.getName() <<
    "\n";
  for (auto &bb : Fn) {
    modified |= nopInsertBB(bb);
  }

  errs() << NopInsertionPass::MyInsertedNops <<
    " inserted nops" << "\n";
  return modified;
}

}

FunctionPass *llvm::createNopInsertionPass() {
  return new NopInsertionPass();
}
