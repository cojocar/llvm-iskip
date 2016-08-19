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

#define DEBUG_TYPE "iskip-lsm-update-replace"

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

#include "LSMUpdateReplacePass.h"
#include "util.h"

STATISTIC(LoadMReplaced, "The # of LoadM replaced");
STATISTIC(StoreMReplaced, "The # of StoreM replaced");
STATISTIC(LSMNotReplaced, "The # of LoadM/StoreM NOT replaced");

using namespace llvm;

cl::opt<bool>
llvm::IskipEnableLSMUpdateReplace("iskip-enable-load-store-update-replace",
                              cl::NotHidden,
                 cl::desc("Enable Load Store Multiple with Update Replace Pass"),
                 cl::init(false));

namespace {
char LSMUpdateReplacePass::ID = 0;

bool LSMUpdateReplacePass::runOnMachineFunction(MachineFunction &Fn)
{
  bool modified = false;

  if (!ARMHardeningUtil::shouldRunPassOnFunction(Fn))
    return false;

  STI = &static_cast<const ARMSubtarget &>(Fn.getSubtarget());
  TII = STI->getInstrInfo();
  TRI = Fn.getSubtarget().getRegisterInfo();

  if (ARMHardeningUtil::IskipEnableVerbose)
    errs() << "ARMHardening(external, LoadStore multiple update removal): " <<
    Fn.getName() << "\n";
  for (auto &bb : Fn) {
    modified |= LSMUpdateReplaceBB(bb);
  }

  if (ARMHardeningUtil::IskipEnableVerbose)
    errs() << LSMUpdateReplacePass::MyLSMUpdateReplaced <<
      "LSM updated replaced" << "\n";
  return modified;
}


bool LSMUpdateReplacePass::replaceOpcodeNoUpdate(MachineBasicBlock::iterator &MBBI)
{
  unsigned newOpcode;
  switch (MBBI->getDesc().getOpcode()) {
  default: return false;
  case ARM::t2LDMIA_UPD:
  case ARM::t2LDMIA_RET:
    newOpcode = ARM::t2LDMIA;
    break;
  case ARM::tLDMIA_UPD:
    newOpcode = ARM::tLDMIA;
    break;
  case ARM::t2LDMDB_UPD:
    newOpcode = ARM::t2LDMDB;
    break;
  case ARM::t2STMIA_UPD:
  case ARM::tSTMIA_UPD:
    newOpcode = ARM::t2STMIA;
    break;
  case ARM::t2STMDB_UPD:
    newOpcode = ARM::t2STMDB;
    break;
  }

  // always remove the first operand as this is the update register
  MBBI->RemoveOperand(0);

  // update the new opcode
  MBBI->setDesc(TII->get(newOpcode));

  return true;
}

bool LSMUpdateReplacePass::getDestRegFromUpd(MachineBasicBlock::iterator &MBBI,
                                             unsigned &DestReg)
{
  const MachineOperand &MO = MBBI->getOperand(0);
  if (MO.isReg() && MO.getReg()) {
    DestReg = MO.getReg();
    return true;
  }
  return false;
}

bool LSMUpdateReplacePass::isRegDefine(MachineBasicBlock::iterator &MBBI,
                                       unsigned Register
                                      )
{
  for (unsigned idx = 0; idx < MBBI->getNumOperands(); ++idx) {
    const MachineOperand &MO = MBBI->getOperand(idx);
    //errs() << "MO=" << MO << "\n";
    if (MO.isReg() && MO.getReg() && MO.isDef()) {
      if (MO.getReg() == Register)
        return true;
    }
  }
  return false;
}

bool LSMUpdateReplacePass::getRegCntFromUpd(MachineBasicBlock::iterator &MBBI,
                                            unsigned &RegCnt, bool isStoreM)
{
  // XXX: ok, this function is unexpectedly complicated
  unsigned cnt = 0;
  for (unsigned idx = 0; idx < MBBI->getNumOperands(); ++idx) {
    const MachineOperand &MO = MBBI->getOperand(idx);
    if (ARMHardeningUtil::IskipEnableVerbose)
      errs() << "MO=" << MO << "\n";
    if (MO.isReg() && MO.getReg()) {
      if (!isStoreM) {
        // we should count a register only if it is a def
        if (MO.isDef()) {
          ++cnt;
        }
      } else {
        ++cnt;
      }
    }
  }

  if (cnt <= 1)
    return false;

  // the dest has been removed already
  if (!isStoreM)
    RegCnt = cnt;
  else
    RegCnt = cnt-1;
  return true;
}

bool LSMUpdateReplacePass::LSMUpdateReplace(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator &MBBI)
{
  bool changed = false;
  //if (!STI->isThumb2())
  //  return false;

  if (ARMHardeningUtil::isInsideITBundle(MBBI))
    return false;

  unsigned SpareReg;
  if (!ARMHardeningUtil::getSpareRegister(TII, ARM::LR, MBB, MBBI,
                                          SpareReg, false, false)) {
    if (ARMHardeningUtil::IskipEnableVerbose)
      errs() << "failed to get a spare reg for: " << *MBBI << "\n";
    return false;
  }


  MachineFunction &MF = *(MBB.getParent());

  MachineBasicBlock::instr_iterator I = MBBI->getIterator();
  MachineBasicBlock::instr_iterator E = MBBI->getParent()->instr_end();

  MachineBasicBlock::iterator firstInsn = MBBI;

  if (ARMHardeningUtil::IskipEnableVerbose)
    errs() << "UPD" << *MBBI << "\n";

  // we don't really know when we run this pass. At some point, the
  // Bundles are transformed to normal insns.
  assert(!MBBI->isInsideBundle());
  assert(!MBBI->isBundled());
  assert(!I->isInsideBundle());
  assert(!I->isBundled());
  // this is the lowest level way to check if an instruction is inside
  // bundle
  assert(!ARMHardeningUtil::isInsideITBundle(MBBI));

  bool isIncr = insnIncrements(MBBI);
  bool wasStoreM = isStoreM(MBBI);
  if (!replaceOpcodeNoUpdate(MBBI)) {
    if (ARMHardeningUtil::IskipEnableVerbose)
      errs() << "no replace opcode for: " << *MBBI << "\n";
    return false;
    llvm_unreachable("end0");
  }
  if (ARMHardeningUtil::IskipEnableVerbose)
    errs() << "UPD_AFTER" << *MBBI << "\n";

  if (isRegDefine(MBBI, SpareReg)) {
    // XXX: this is a hack, we should be able to allocate registers form
    // the normal pool
    unsigned SpareRegAlt = SpareReg == ARM::R10 ? ARM::R12 : ARM::R10;
    if (isRegDefine(MBBI, SpareRegAlt)) {
      if (ARMHardeningUtil::IskipEnableVerbose)
        errs() << "XXX: we do not have enought spare registers\n";
      return false;
    }
    SpareReg = SpareRegAlt;
  }

  unsigned DestReg;
  unsigned RegCnt;

  if (!getDestRegFromUpd(MBBI, DestReg)) {
    if (ARMHardeningUtil::IskipEnableVerbose)
      errs() << "no dest reg: " << *MBBI << "\n";
    llvm_unreachable("end1");
  }

  if (!getRegCntFromUpd(MBBI, RegCnt, wasStoreM)) {
    if (ARMHardeningUtil::IskipEnableVerbose)
      errs() << "no reg cnt: " << *MBBI << "\n";
    llvm_unreachable("end2");
  }
  if (ARMHardeningUtil::IskipEnableVerbose)
    errs() << "UPD_CNT" << *MBBI << "has " << RegCnt << "\n";

  DebugLoc DL = MBBI->getDebugLoc();

  MachineOperand &MO = MBBI->getOperand(0);

  assert(MO.isReg() && MO.getReg());
  assert(DestReg == MO.getReg());

  MO.setReg(SpareReg);

  if (wasStoreM) {
    // don't treat this instruction as frame setup because we trigger
    // some incompatible opts. The opts assume STMIA_UPD insn.
    MBBI->clearFlag(MachineInstr::FrameSetup);
  }

  auto addOrSub =
    BuildMI(MBB, firstInsn, DL, TII->get(isIncr ? ARM::t2ADDri12 :
                                         ARM::t2SUBri12))
    .addReg(SpareReg, RegState::Define)
    .addReg(DestReg)
    .addImm((unsigned)4*RegCnt)
    .addImm((unsigned)ARMCC::AL)
    .addReg(0)
    .setMIFlags(0);

  auto mov =
    BuildMI(MBB, firstInsn, DL, TII->get(ARM::tMOVr))
    .addReg(DestReg)
    .addReg(SpareReg, RegState::Kill)
    .addImm((unsigned)ARMCC::AL)
    .addReg(0)
    .setMIFlags(0);

  auto subOrAdd =
    BuildMI(MBB, firstInsn, DL, TII->get(isIncr ? ARM::t2SUBri12 :
                                         ARM::t2ADDri12))
    .addReg(SpareReg, RegState::Define)
    .addReg(DestReg)
    .addImm((unsigned)4*RegCnt)
    .addImm((unsigned)ARMCC::AL)
    .addReg(0)
    .setMIFlags(0);

  changed = true;

  wasStoreM ? ++StoreMReplaced : ++LoadMReplaced;

  return changed;
}

bool LSMUpdateReplacePass::LSMUpdateReplaceBB(MachineBasicBlock &MBB)
{
  MachineBasicBlock::iterator MBBI;
  bool modified = false;

  if (MBB.begin() == MBB.end())
    return false;

  SmallVector<MachineInstr *, 4> eraseMe;

  for (MBBI = MBB.begin(); MBBI != MBB.end(); ++MBBI) {
    //errs() << "QQQ " << *MBBI << MBBI->isBundle() << MBBI->isBundled() <<
    //  MBBI->isBundledWithPred() << MBBI->isBundledWithSucc() <<
    //  MBBI->isInsideBundle() << " " <<
    //  ARMHardeningUtil::isInsideITBundle(MBBI) << "\n";
    if ((isLoadM(MBBI) || isStoreM(MBBI)) &&
        !ARMHardeningUtil::isInsideITBundle(MBBI)) {
      bool m = LSMUpdateReplace(MBB, MBBI);
      LSMNotReplaced += !m;
      modified |= m;
      if (m) {
        ++LSMUpdateReplacePass::MyLSMUpdateReplaced;
        eraseMe.push_back(&(*MBBI));
        //break;
      } else {
        errs() << "No replacement for: " << *MBBI << "\n";
      }
    } else {
    }
  }

  return modified;
}

bool LSMUpdateReplacePass::isLoadM(MachineBasicBlock::iterator &MBBI)
{
  switch (MBBI->getDesc().getOpcode()) {
  default: return false;
  case ARM::t2LDMDB_UPD:
  case ARM::t2LDMIA_UPD:
  case ARM::tLDMIA_UPD:
  case ARM::t2LDMIA_RET:
           break;
  }
  return true;
}

bool LSMUpdateReplacePass::isStoreM(MachineBasicBlock::iterator &MBBI)
{
  switch (MBBI->getDesc().getOpcode()) {
  default: return false;
  case ARM::t2STMDB_UPD:
  case ARM::t2STMIA_UPD:
  case ARM::tSTMIA_UPD:
           break;
  }
  return true;
}

bool LSMUpdateReplacePass::insnIncrements(MachineBasicBlock::iterator &MBBI)
{
  switch (MBBI->getDesc().getOpcode()) {
  default: llvm_unreachable("undefined op while searching for incr");
  case ARM::t2STMIA_UPD:
  case ARM::tSTMIA_UPD:
  case ARM::t2LDMIA_UPD:
  case ARM::tLDMIA_UPD:
  case ARM::t2LDMIA_RET:
           return true;
  case ARM::t2STMDB_UPD:
  case ARM::t2LDMDB_UPD:
           return false;
  }
  return false;
}

}

FunctionPass *llvm::createLSMUpdateReplacePass() {
  return new LSMUpdateReplacePass();
}
