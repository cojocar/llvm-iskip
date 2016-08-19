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

#define DEBUG_TYPE "iskip-it-replace"

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

#include "ITReplacePass.h"
#include "util.h"

STATISTIC(ITReplaced, "The # of IT replaced");
STATISTIC(ITNotReplaced, "The # of IT NOT replaced");

using namespace llvm;

cl::opt<bool>
llvm::IskipEnableITReplace("iskip-enable-it-replace",
                 cl::desc("Enable ITT Replace Pass"),
                 cl::init(false));

namespace {
char ITReplacePass::ID = 0;

bool ITReplacePass::runOnMachineFunction(MachineFunction &Fn)
{
  bool modified = false;

  if (!ARMHardeningUtil::shouldRunPassOnFunction(Fn))
    return false;

  STI = &static_cast<const ARMSubtarget &>(Fn.getSubtarget());
  TII = STI->getInstrInfo();
  TRI = Fn.getSubtarget().getRegisterInfo();

  if (ARMHardeningUtil::IskipEnableVerbose)
    errs() << "ARMHardening(external, it removal): " <<
    Fn.getName() << "\n";
  for (auto &bb : Fn) {
    modified |= ITReplaceBB(bb);
  }

  if (ARMHardeningUtil::IskipEnableVerbose)
    errs() << ITReplacePass::MyITReplaced <<
    "it replaced" << "\n";

  return modified;
}

bool ITReplacePass::harvestInsnFromIT(MachineBasicBlock::instr_iterator &itInsn,
                         SmallVector<MachineInstr *, 4> &ThenInsns,
                         SmallVector<MachineInstr *, 4> &ElseInsns,
                         unsigned &Cond)
{
  MachineBasicBlock::instr_iterator bundledInsn = itInsn;

  unsigned Firstcond = itInsn->getOperand(0).getImm();
  unsigned Mask = itInsn->getOperand(1).getImm();

  unsigned CondBit0 = Firstcond & 1;
  unsigned NumTZ = countTrailingZeros<uint8_t>(Mask);
  Cond = static_cast<unsigned>(Firstcond & 0xf);
  assert(NumTZ <= 3 && "Invalid IT mask!");
  ++bundledInsn;
  // push condition codes onto the stack the correct order for the pops
  for (unsigned Pos = NumTZ+1; Pos <= 3; ++Pos) {
    bool T = ((Mask >> Pos) & 1) == CondBit0;
    if (T) {
      //errs() << "cc " << *bundledInsn << " " << bundledInsn->isInsideBundle() << "\n";
      ThenInsns.push_back(&(*bundledInsn));
    } else {
      //errs() << "!cc " << *bundledInsn << " " << bundledInsn->isInsideBundle() << "\n";
      ElseInsns.push_back(&(*bundledInsn));
    }
    ++bundledInsn;
  }
  ThenInsns.push_back(&(*bundledInsn));
  //errs() << "cc " << *bundledInsn << " " << bundledInsn->isInsideBundle() << "\n";
  return true;
}

MachineInstr* ITReplacePass::copyInstructionFromBundle(MachineInstr *I)
{
  auto insn = I->removeFromBundle();

  for (unsigned idx = 0; idx < insn->getNumOperands(); ++idx) {
    MachineOperand &MO = insn->getOperand(idx);
    if (MO.isReg() && !MO.isTied() && (MO.getReg() == ARM::CPSR ||
                                       MO.getReg() == ARM::ITSTATE)) {
      // remove cpsr and itstate meaning
      MO.setIsUndef();
    }
  }

  return insn;
}

bool ITReplacePass::BundleReplace(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator &MBBI)
{
  bool changed = false;
  //if (!STI->isThumb2())
  //  return false;

  //assert(MBBI->isBundle() && "Expecting a bundle");
  assert(ARMHardeningUtil::isInsideITBundle(MBBI));

  MachineFunction &MF = *(MBB.getParent());

  MachineBasicBlock::instr_iterator I = MBBI->getIterator();
  MachineBasicBlock::instr_iterator E = MBBI->getParent()->instr_end();
  MachineBasicBlock::instr_iterator itInsn = I;
  MachineBasicBlock::instr_iterator bundledInsn;
  ++itInsn;

  // this is the instruction after the bundle
  MachineBasicBlock::iterator firstInsn = MBBI;
  ++firstInsn;

  //bundledInsn = itInsn;
  assert(itInsn->getOpcode() == ARM::t2IT);

  SmallVector<MachineInstr *, 4> ThenInsn, ElseInsn;
  unsigned Cond;
  harvestInsnFromIT(itInsn, ThenInsn, ElseInsn, Cond);
  assert(ThenInsn.size() + ElseInsn.size() == MBBI->getBundleSize());

  if (ARMHardeningUtil::IskipEnableVerbose)
    errs() << "Cond: " << ARMCondCodeToString((ARMCC::CondCodes)Cond) << "\n";
  for (auto I : ThenInsn) {
    if (ARMHardeningUtil::IskipEnableVerbose)
      errs() << "Then: " << *I << "\n";
  }
  for (auto I : ElseInsn) {
    if (ARMHardeningUtil::IskipEnableVerbose)
      errs() << "Else: " << *I << "\n";
  }


  auto MLI = getAnalysisIfAvailable<MachineLoopInfo>();
  MachineBasicBlock *RestMBB;
  bool retry = false;

retry_split:
  // This is a hack. The block splitting can fail because firstInsn
  // points outside of this block (i.e. we try to split after the last
  // instruction). The hack is to add a nop instruction and retry
  // splitting.
  RestMBB = ARMHardeningUtil::SplitMBBAt(TII,
                                         MLI,
                                         MBB, firstInsn);
  if (RestMBB == nullptr && retry) {
    if (ARMHardeningUtil::IskipEnableVerbose)
      errs() << "failed to split BB: " << MBB << "\n";
    return false;
  }
  if (RestMBB == nullptr && !retry) {
    BuildMI(MBB, MBB.end(), MBBI->getDebugLoc(),
            TII->get(ARM::tHINT))
      .addImm(0)
      .addImm((unsigned)ARMCC::AL)
      .addReg(0);
    retry = true;
    firstInsn = MBBI;
    ++firstInsn;
    goto retry_split;
  }

  if (retry) {
    if (ARMHardeningUtil::IskipEnableVerbose)
      errs() << "nop_hack\n";
  }

  assert(RestMBB != nullptr);

  const BasicBlock *BB = MBB.getBasicBlock();
  DebugLoc DL = MBBI->getDebugLoc();
  assert(BB != nullptr);

  MachineFunction::iterator MBBIt = MBB.getIterator();
  ++MBBIt;

  // construct the Else MBB
  MachineBasicBlock *ElseMBB = MF.CreateMachineBasicBlock(BB);
  MF.insert(MBBIt, ElseMBB);
  for (auto I : ElseInsn) {
    auto insn = copyInstructionFromBundle(I);
    ElseMBB->push_back(insn);
  }
  BuildMI(*ElseMBB, ElseMBB->end(), DL, TII->get(ARM::t2B))
    .addMBB(RestMBB)
    .addImm((int64_t)ARMCC::AL)
    .addReg(0);
  ElseMBB->addSuccessor(RestMBB, BranchProbability::getOne());
  if (MLI)
    if (MachineLoop *ML = MLI->getLoopFor(&MBB))
      ML->addBasicBlockToLoop(ElseMBB, MLI->getBase());
  if (ARMHardeningUtil::IskipEnableVerbose)
    errs() << "inserted else\n";

  // construct the Then BB
  MachineBasicBlock *ThenMBB = MF.CreateMachineBasicBlock(BB);
  MF.insert(MBBIt, ThenMBB);
  for (auto I : ThenInsn) {
    auto insn = copyInstructionFromBundle(I);
    ThenMBB->push_back(insn);
  }
  BuildMI(*ThenMBB, ThenMBB->end(), DL, TII->get(ARM::t2B))
    .addMBB(RestMBB)
    .addImm((int64_t)ARMCC::AL)
    .addReg(0);
  ThenMBB->addSuccessor(RestMBB, BranchProbability::getOne());
  if (MLI)
    if (MachineLoop *ML = MLI->getLoopFor(&MBB))
      ML->addBasicBlockToLoop(ThenMBB, MLI->getBase());
  if (ARMHardeningUtil::IskipEnableVerbose)
    errs() << "inserted then\n";



  // fixup the curent MBB
  MBB.removeSuccessor(RestMBB);
  MBB.addSuccessor(ThenMBB);
  MBB.addSuccessor(ElseMBB);

  //BuildMI(MBB, MBB.getLastNonDebugInstr(), DL, TII->get(ARM::t2Bcc))
  BuildMI(MBB, MBB.end(), DL, TII->get(ARM::t2Bcc))
    .addMBB(ThenMBB)
    .addImm((int64_t)Cond)
    .addReg(0);

  // it falls through
  //BuildMI(MBB, MBB.end(), DL, TII->get(ARM::t2B))
  //  .addMBB(ElseMBB)
  //  .addImm((int64_t)ARMCC::AL)
  //  .addReg(0);

  MBB.erase(MBBI);
  changed = true;
  ++ITReplaced;

  return changed;
}

bool ITReplacePass::ITReplaceBB(MachineBasicBlock &MBB)
{
  MachineBasicBlock::iterator MBBI;
  bool modified = false;

  if (MBB.begin() == MBB.end())
    return false;

  for (MBBI = MBB.begin(); MBBI != MBB.end(); ++MBBI) {
    if (MBBI->isBundle()) {
      bool m = BundleReplace(MBB, MBBI);
      ITNotReplaced += !m;
      modified |= m;
      if (m) {
        ++ITReplacePass::MyITReplaced;
        break;
      } else {
        errs() << "No replacement for: " << *MBBI << "\n";
      }
    } else {
    }
  }

  return modified;
}

bool ITReplacePass::isIT(MachineBasicBlock::iterator &MBBI)
{
  switch (MBBI->getDesc().getOpcode()) {
  default: return false;
  case ARM::t2IT:
           break;
  }
  return true;
}

}

FunctionPass *llvm::createITReplacePass() {
  return new ITReplacePass();
}
