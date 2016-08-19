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

#define DEBUG_TYPE "iskip-call-insertion"

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

#include "CallInsertionPass.h"
#include "util.h"

STATISTIC(InsertedNops, "The # of calls inserted");

using namespace llvm;

namespace llvm {

cl::opt<IskipCallInsertion::InstructionToBeInserted>
  IskipEnableCallInsertion("iskip-enable-call-insertion", cl::NotHidden,
          cl::desc("Enable call insertion. Specify the insn used."),
          cl::values(
            clEnumValN(IskipCallInsertion::Insn_none,
                       "none", "disable this plugin"),
            clEnumValN(IskipCallInsertion::Insn_udf,
                       "udf" , "use udf.w #43 insn"),
            clEnumValN(IskipCallInsertion::Insn_nop,
                       "nop" , "insert two nops"),
            clEnumValEnd),
          cl::init(IskipCallInsertion::Insn_none));
}

namespace {
char CallInsertionPass::ID = 0;
bool CallInsertionPass::callInsert(MachineBasicBlock &MBB,
                                           MachineBasicBlock::iterator
                                           &MBBI)
{
  ++InsertedNops;
  ++CallInsertionPass::MyInsertedNops;

#if 0
  BuildMI(MBB, MBBI, MBBI->getDebugLoc(),
          TII->get(ARM::t2MOVr))
    .addReg(ARM::R0, RegState::Undef) /* Ra */
    .addImm((unsigned)ARMCC::AL)
    .addReg(ARM::R0, RegState::Undef)
    .addReg(0)
    .setMIFlags(0);
#endif
/////  BuildMI(MBB, MBBI, MBBI->getDebugLoc(),
/////          TII->get(ARM::tHINT))
/////    .addImm(0)
/////    .addImm((unsigned)ARMCC::AL)
/////    .addReg(0);
/////
#if 0
  DebugLoc DL = MBBI->getDebugLoc();
  ///auto movR0PC = BuildMI(MBB, MBBI, DL, TII->get(ARM::t2MOVr))
  ///  .addReg(ARM::R0, RegState::Define)
  ///  .addReg(ARM::PC)
  ///  .addReg(ARM::R0)
  ///  .addImm((int64_t)ARMCC::AL)
  ///  .addReg(0);
  //
  /////unsigned SpareReg;
  /////if (!ARMHardeningUtil::getSpareRegister(TII, ARM::LR, MBB, MBBI, SpareReg)) {
  /////  errs() << "failed to get a spare reg for: " << *MBBI << "\n";
  /////  return false;
  /////}

  /////auto mov = AddDefaultPred(BuildMI(MBB, MBBI, DL, TII->get(ARM::t2MOVr),
  /////                   SpareReg)
  /////  .addReg(ARM::LR, RegState::Implicit)
  /////  .addReg(0));

  auto bl = BuildMI(MBB, MBBI, DL, TII->get(ARM::tBL))
    .addImm((int64_t)ARMCC::AL)
    .addReg(0);

  ///auto movBack = AddDefaultPred(BuildMI(MBB, MBBI, DL, TII->get(ARM::t2MOVr),
  ///                       ARM::LR)
  ///  .addReg(SpareReg, RegState::Kill)
  ///  .addReg(0));

  bl.addExternalSymbol("my_signal");

  //bl.addReg(ARM::R0, RegState::ImplicitKill);
  //bl.addReg(ARM::R0, RegState::ImplicitKill);
  bl.addReg(ARM::LR, RegState::ImplicitDefine);
  //bl.addReg(ARM::LR, RegState::Implicit);
  //bl.addReg(ARM::LR, RegState::Kill);
  bl.addRegMask(TRI->getCallPreservedMask(*MBB.getParent(), CallingConv::C));
  //SmallVector<unsigned, 4> UsedRegs;
  //UsedRegs.push_back(ARM::LR);
  //static_cast<MachineInstr *>(bl)->setPhysRegsDeadExcept(UsedRegs, *TRI);
  /////BuildMI(MBB, MBBI, MBBI->getDebugLoc(),
  /////        TII->get(ARM::tHINT))
  /////  .addImm(0)
  /////  .addImm((unsigned)ARMCC::AL)
  /////  .addReg(0);
  /////BuildMI(MBB, MBBI, MBBI->getDebugLoc(),
  /////        TII->get(ARM::tHINT))
  /////  .addImm(0)
  /////  .addImm((unsigned)ARMCC::AL)
  /////  .addReg(0);
  /////BuildMI(MBB, MBBI, MBBI->getDebugLoc(),
  /////        TII->get(ARM::tHINT))
  /////  .addImm(0)
  /////  .addImm((unsigned)ARMCC::AL)
  /////  .addReg(0);
  /////BuildMI(MBB, MBBI, MBBI->getDebugLoc(),
  /////        TII->get(ARM::tHINT))
  /////  .addImm(0)
  /////  .addImm((unsigned)ARMCC::AL)
  /////  .addReg(0);

#endif

  DebugLoc DL = MBBI->getDebugLoc();
  if (IskipEnableCallInsertion == IskipCallInsertion::Insn_udf) {
    BuildMI(MBB, MBBI, DL, TII->get(ARM::t2UDF)).addImm(43);
  } else if (IskipEnableCallInsertion == IskipCallInsertion::Insn_nop) {
    BuildMI(MBB, MBBI, DL,
            TII->get(ARM::tHINT))
      .addImm(0)
      .addImm((unsigned)ARMCC::AL)
      .addReg(0);
    BuildMI(MBB, MBBI, DL,
            TII->get(ARM::tHINT))
      .addImm(0)
      .addImm((unsigned)ARMCC::AL)
      .addReg(0);
  } else {
    llvm_unreachable("CallInsertion Pass invoked but no instruction was specified");
  }

  return true;
}

bool CallInsertionPass::callInsertBB(MachineBasicBlock &MBB)
{
  MachineBasicBlock::iterator MBBI;
  bool modified = false;

  if (MBB.begin() == MBB.end())
    return false;

  for (MBBI = MBB.begin(); MBBI != MBB.end(); ++MBBI) {
    if (ARMHardeningUtil::isInsideITBundle(MBBI))
      continue;
    callInsert(MBB, MBBI);
    modified = true;
  }

  return modified;
}

bool CallInsertionPass::runOnMachineFunction(MachineFunction &Fn)
{
  bool modified = false;

  if (!ARMHardeningUtil::shouldRunPassOnFunction(Fn))
    return false;

  STI = &static_cast<const ARMSubtarget &>(Fn.getSubtarget());
  TII = STI->getInstrInfo();
  TRI = Fn.getSubtarget().getRegisterInfo();

  errs() << "ARMHardening(external, call insertion): " << Fn.getName() <<
    "\n";
  for (auto &bb : Fn) {
    modified |= callInsertBB(bb);
  }

  errs() << CallInsertionPass::MyInsertedNops <<
    " inserted calls" << "\n";
  return modified;
}

}

FunctionPass *llvm::createCallInsertionPass() {
  return new CallInsertionPass();
}
