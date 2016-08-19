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

#define DEBUG_TYPE "iskip-push-pop-replace"

#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/CodeGen/MachineInstrBuilder.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetInstrInfo.h>
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Pass.h"
#include "llvm/MC/MCInst.h"
#include "llvm/ADT/Statistic.h"
#include "llvm-src/include/llvm/CodeGen/MachineConstantPool.h"
#include "llvm-src/lib/Target/ARM/ARMMachineFunctionInfo.h"
#include "llvm-src/lib/Target/ARM/ARMConstantPoolValue.h"

#include "PushPopReplacePass.h"
#include "util.h"

STATISTIC(PushReplaced, "The # of push replaced");
STATISTIC(PopReplaced, "The # of pops replaced");
STATISTIC(PushPopNotReplaced, "The # of push/pop not replaced");

using namespace llvm;

cl::opt<bool>
llvm::IskipEnablePushPopReplace("iskip-enable-push-pop-replace",
                              cl::NotHidden,
                 cl::desc("Enable Push Pop Replace Pass"),
                 cl::init(false));

namespace {
char PushPopReplacePass::ID = 0;

bool PushPopReplacePass::runOnMachineFunction(MachineFunction &Fn)
{
  bool modified = false;

  if (!ARMHardeningUtil::shouldRunPassOnFunction(Fn))
    return false;

  STI = &static_cast<const ARMSubtarget &>(Fn.getSubtarget());
  TII = STI->getInstrInfo();
  TRI = Fn.getSubtarget().getRegisterInfo();

  if (ARMHardeningUtil::IskipEnableVerbose)
    errs() << "ARMHardening(external, push/pop removal): " <<
      Fn.getName() << "\n";
  for (auto &bb : Fn) {
    modified |= pushPopReplaceBB(bb);
  }

  if (ARMHardeningUtil::IskipEnableVerbose)
    errs() << PushPopReplacePass::MyPushPopReplaced <<
      "push pop replaced" << "\n";
  return modified;
}

bool PushPopReplacePass::emitPopReplacement(
  MachineBasicBlock &MBB,
  MachineBasicBlock::iterator &firstInsn,
  unsigned SpareReg,
  SmallVector<unsigned, 4> &SavedGPRs,
  unsigned cc,
  const DebugLoc &DL)
{
  if (SavedGPRs.size() < 1) {
    return false;
    //errs() << "failed to get GPRs" << firstInsn << "\n";
    assert(SavedGPRs.size() >= 1 && "At least one register is needed in push/pop");
  }

  auto add =
    BuildMI(MBB, firstInsn, DL, TII->get(ARM::t2ADDri12))
    .addReg(SpareReg, RegState::Define)
    .addReg(ARM::SP)
    .addImm((unsigned)4*SavedGPRs.size())
    .addImm(cc)
    .addReg(0)
    .setMIFlags(0);

  auto mov =
    BuildMI(MBB, firstInsn, DL, TII->get(ARM::tMOVr))
    .addReg(ARM::SP)
    .addReg(SpareReg, RegState::Kill)
    .addImm(cc)
    .addReg(0)
    .setMIFlags(0);

  auto sub =
    BuildMI(MBB, firstInsn, DL, TII->get(ARM::t2SUBri12))
    .addReg(SpareReg, RegState::Define)
    .addReg(ARM::SP)
    .addImm((unsigned)4*SavedGPRs.size())
    .addImm(cc)
    .addReg(0)
    .setMIFlags(0);

  auto ldmia =
    BuildMI(MBB, firstInsn, DL, TII->get(ARM::t2LDMIA))
    .addReg(SpareReg, RegState::Kill)
    .addImm(cc)
    .addReg(0);

  //for (auto it = SavedGPRs.rbegin(); it != SavedGPRs.rend(); ++it) {
  //for (auto reg : SavedGPRs) {
  //for (auto it = SavedGPRs.rbegin(); it != SavedGPRs.rend(); ++it) {
  //  ldmdb.addReg(*it);
  //  ///errs() << "rev" << *it << "\n";
  //}

  for (auto reg : SavedGPRs) {
    ldmia.addReg(reg);
  }

  ldmia.setMIFlags(0);
  return true;
}

bool PushPopReplacePass::emitPushReplacement(
  MachineBasicBlock &MBB,
  MachineBasicBlock::iterator &firstInsn,
  unsigned SpareReg,
  SmallVector<unsigned, 4> &SavedGPRs,
  unsigned cc,
  const DebugLoc &DL)
{
  if (SavedGPRs.size() < 1) {
    return false;
    //errs() << "failed to get GPRs" << firstInsn << "\n";
    assert(SavedGPRs.size() >= 1 && "At least one register is needed in push/pop");
  }

  auto stmdb =
    BuildMI(MBB, firstInsn, DL, TII->get(ARM::t2STMDB))
    .addReg(ARM::SP)
    .addImm(cc)
    .addReg(0);

  for (unsigned reg : SavedGPRs)
    stmdb.addReg(reg);

  stmdb.setMIFlags(0);

  auto sub =
    BuildMI(MBB, firstInsn, DL, TII->get(ARM::t2SUBri12))
    .addReg(SpareReg, RegState::Define)
    .addReg(ARM::SP)
    .addImm((unsigned)4*SavedGPRs.size())
    .addImm(cc)
    .addReg(0)
    .setMIFlags(0);

  auto mov =
    BuildMI(MBB, firstInsn, DL, TII->get(ARM::tMOVr))
    .addReg(ARM::SP)
    .addReg(SpareReg, RegState::Kill)
    .addImm(cc)
    .addReg(0)
    .setMIFlags(0);
  return true;
}

bool PushPopReplacePass::pushPopReplace(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator &MBBI)
{
  bool changed = false;
  if (!STI->isThumb2())
    return false;

  if (ARMHardeningUtil::isInsideITBundle(MBBI))
    return false;

  unsigned SpareReg;
  if (!ARMHardeningUtil::getSpareRegister(TII, ARM::LR, MBB, MBBI, SpareReg, false)) {
    if (ARMHardeningUtil::IskipEnableVerbose)
      errs() << "failed to get a spare reg for: " << *MBBI << "\n";
    return false;
  }

  MachineBasicBlock::iterator lastInsn = MBBI;
  MachineBasicBlock::iterator firstInsn = MBBI;
  ++firstInsn;

  SmallVector<unsigned, 4> SavedGPRs;

  unsigned cc = (unsigned)ARMCC::AL;
  //unsigned opSize = MBBI->getDesc().getOpcode() == ARM::tPOP_RET ?
  //  MBBI->getNumOperands() : MBBI->getNumOperands()-2;
  unsigned opSize = isPop(MBBI) ? MBBI->getNumOperands() :
    MBBI->getNumOperands()-2;
  bool isPop = this->isPop(MBBI);

  for (unsigned idx = 0; idx < opSize; ++idx) {
    const MachineOperand &MO = MBBI->getOperand(idx);
    // only defs
    if (MO.isReg() && MO.getReg() && MO.getReg() != ARM::CPSR && (!isPop || MO.isDef())) {
      //errs() << "RR" << MO.getReg() << " " << MO.getSubReg() << "\n";
      SavedGPRs.push_back(MO.getReg());
    } else if (MO.isImm()) {
      cc = MO.getImm();
    }
  }

  if (isPop) {
    changed = emitPopReplacement(MBB, firstInsn, SpareReg, SavedGPRs,
                                 cc, MBBI->getDebugLoc());
    //errs() << "pop_replaced: " << *MBBI << "\n";
    PopReplaced += !!changed;
  } else {
    changed = emitPushReplacement(MBB, firstInsn, SpareReg, SavedGPRs,
                                  cc, MBBI->getDebugLoc());
    PushReplaced += !!changed;
  }

  return changed;
}

bool PushPopReplacePass::pushPopReplaceBB(MachineBasicBlock &MBB)
{
  MachineBasicBlock::iterator MBBI;
  bool modified = false;

  if (MBB.begin() == MBB.end())
    return false;

  SmallVector<MachineInstr *, 4> eraseMe;

  for (MBBI = MBB.begin(); MBBI != MBB.end(); ++MBBI) {
    if (isPush(MBBI) || isPop(MBBI)) {
      bool m = pushPopReplace(MBB, MBBI);
      PushPopNotReplaced += !m;
      modified |= m;
      if (m) {
        ++PushPopReplacePass::MyPushPopReplaced;
        eraseMe.push_back(&(*MBBI));
        //break;
      } else {
        errs() << "No replacement for: " << *MBBI << "\n";
      }
    } else {
    }
  }

  for (auto eraseInsn : eraseMe)
    MBB.erase(eraseInsn);

  return modified;
}

bool PushPopReplacePass::isPush(MachineBasicBlock::iterator &MBBI)
{
  switch (MBBI->getDesc().getOpcode()) {
  default: return false;
  case ARM::tPUSH:
           break;
  }
  return true;
}

bool PushPopReplacePass::isPop(MachineBasicBlock::iterator &MBBI)
{
  switch (MBBI->getDesc().getOpcode()) {
  default: return false;
  case ARM::tPOP:
  case ARM::tPOP_RET:
           break;
  }
  return true;
}

}

FunctionPass *llvm::createPushPopReplacePass() {
  return new PushPopReplacePass();
}
