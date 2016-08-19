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

#define DEBUG_TYPE "iskip-bl-replace"

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

#include "BLReplacePass.h"
#include "util.h"

STATISTIC(BLReplaced, "The # of BL replaced");
STATISTIC(BLNotReplaced, "The # of not replaced");

using namespace llvm;

cl::opt<bool>
llvm::IskipEnableBLReplace("iskip-enable-bl-replace",
                 cl::desc("Enable BL Replace Pass"),
                 cl::init(false));

namespace {
char BLReplacePass::ID = 0;

bool BLReplacePass::runOnMachineFunction(MachineFunction &Fn)
{
  bool modified = false;
  RetBB = nullptr;

  if (!ARMHardeningUtil::shouldRunPassOnFunction(Fn))
    return false;

  STI = &static_cast<const ARMSubtarget &>(Fn.getSubtarget());
  TII = STI->getInstrInfo();
  TRI = Fn.getSubtarget().getRegisterInfo();

  if (ARMHardeningUtil::IskipEnableVerbose)
    errs() << "ARMHardening(external, bl removal): " <<
      Fn.getName() << "\n";
  for (auto &bb : Fn) {
    modified |= blRemovalBB(bb);
  }

  if (ARMHardeningUtil::IskipEnableVerbose)
    errs() << BLReplacePass::MyBLReplaced <<
      " bl replaced" << "\n";
  return modified;
}

bool BLReplacePass::blRemoval(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator &MBBI)
{
  if (!STI->isThumb2())
    return false;

  if (ARMHardeningUtil::isInsideITBundle(MBBI))
    return false;

  unsigned SpareReg;
  if (!ARMHardeningUtil::getSpareRegister(TII, ARM::LR, MBB, MBBI, SpareReg)) {
    errs() << "failed to get a spare reg for: " << *MBBI << "\n";
    return false;
  }
  int MOSymbolIdx = -1;
  for (unsigned idx = 0; idx < MBBI->getNumOperands(); ++idx) {
    const MachineOperand &MO = MBBI->getOperand(idx);
    if (MO.isGlobal() || MO.isSymbol()) {
      MOSymbolIdx = idx;
      break;
    }
  }
  if (MOSymbolIdx == -1) {
    errs() << "could not find global for BL" << *MBBI << "\n";
    return false;
  }

  MachineBasicBlock::iterator lastInsn = MBBI;
  MachineBasicBlock::iterator firstInsn = MBBI;
  ++firstInsn;

  MachineBasicBlock *NewBB =
  ARMHardeningUtil::SplitMBBAt(TII,
                               getAnalysisIfAvailable<MachineLoopInfo>(),
                               MBB,
                               firstInsn);
  if (NewBB == nullptr) {
    return false;
  }

  // XXX: hack, this can be removed by subsequent opts
  NewBB->setHasAddressTaken();
  //NewBB->setIsEHFuncletEntry();

  DebugLoc DL = MBBI->getDebugLoc();

  // adr rx, newBB
  BuildMI(MBB, lastInsn, DL, TII->get(ARM::t2ADR))
    .addReg(SpareReg, RegState::Define)
    .addMBB(NewBB)
    .addImm((int64_t)ARMCC::AL)
    .addReg(0);

  // add lr, rx, 1 (thumb mode)
  AddDefaultCC(AddDefaultPred(
      BuildMI(MBB, lastInsn, DL, TII->get(ARM::t2ADDri)
             )
      .addReg(ARM::LR, RegState::Define)
      .addReg(SpareReg, RegState::Kill)
      .addImm(1)
      ));

  // load the function target in a register
  auto b = BuildMI(MBB, lastInsn, DL, TII->get(ARM::t2MOVi32imm))
    .addReg(SpareReg, RegState::Define)
    .setMIFlags(0);
  b.addOperand(MBBI->getOperand(MOSymbolIdx));

  BuildMI(*NewBB, firstInsn, MBBI->getDebugLoc(),
          TII->get(ARM::tHINT))
    .addImm(0)
    .addImm((unsigned)ARMCC::AL)
    .addReg(0);

  // emit BX instruction
  bool hasReturn = false;
  auto bx  = BuildMI(MBB, lastInsn, DL, TII->get(ARM::tBX));
  // add implicit regs, generate load for arguments
  for (unsigned idx = 0; idx < MBBI->getNumOperands(); ++idx) {
    const MachineOperand &MO = MBBI->getOperand(idx);
    if (MO.isReg()) {
      if (MO.getReg() && MO.getReg() == ARM::R0 && MO.isDef()) {
        bx.addReg(MO.getReg(), RegState::Implicit | RegState::Define);
        hasReturn = true;
      } else {
        bx.addReg(MO.getReg(), RegState::Implicit);
      }
    }
  }

  // spare reg is kill
  bx.addReg(SpareReg, RegState::Kill)
    .addImm((int64_t)ARMCC::AL)
    .addReg(0);

  // add reg mask, XXX: assume CallingConv
  bx.addRegMask(TRI->getCallPreservedMask(*MBB.getParent(), CallingConv::C));
  SmallVector<unsigned, 4> UsedRegs;
  UsedRegs.push_back(ARM::LR);
  if (hasReturn)
    UsedRegs.push_back(ARM::R0);
  static_cast<MachineInstr *>(bx)->setPhysRegsDeadExcept(UsedRegs, *TRI);

  // remove original BL instruction
  MBB.erase(MBBI);

  return true;
}

bool BLReplacePass::blRemovalBB(MachineBasicBlock &MBB)
{
  MachineBasicBlock::iterator MBBI;
  bool modified = false;

  if (MBB.begin() == MBB.end())
    return false;

  for (MBBI = MBB.begin(); MBBI != MBB.end(); ++MBBI) {
    if (isBL(MBBI)) {
      bool m = blRemoval(MBB, MBBI);
      BLReplaced += !!m;
      BLNotReplaced += !m;
      modified |= m;
      if (m) {
        ++BLReplacePass::MyBLReplaced;
        break;
      } else {
        if (ARMHardeningUtil::IskipEnableVerbose)
          errs() << "No replacement for: " << *MBBI << "\n";
      }
    } else {
    }
  }
  return modified;
}

bool BLReplacePass::isBL(MachineBasicBlock::iterator &MBBI)
{
  switch (MBBI->getDesc().getOpcode()) {
  default: return false;
  case ARM::tBL:
           break;
  }
  return true;
}

}

FunctionPass *llvm::createBLReplacePass() {
  return new BLReplacePass();
}
