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

#define DEBUG_TYPE "iskip-lsv"

#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/CodeGen/MachineInstrBuilder.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetInstrInfo.h>
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/Pass.h"
#include "llvm/MC/MCInst.h"
#include "llvm/ADT/Statistic.h"
#include "llvm-src/include/llvm/CodeGen/MachineConstantPool.h"
#include "llvm-src/lib/Target/ARM/ARMMachineFunctionInfo.h"
#include "llvm-src/lib/Target/ARM/ARMConstantPoolValue.h"

#include "LoadStoreVerificationPass.h"
#include "util.h"

STATISTIC(LoadStoresVerified, "The # of load/store instrumented");
STATISTIC(StoresNotInstrumented, "The # of store NOT instrumented");
STATISTIC(LoadsNotInstrumented, "The # of loads NOT instrumented");

//
//grep t2LDR
//~/code/llvm-dev/llvm-build/lib/Target/ARM/ARMGenInstrInfo.inc | grep
//-v '{' | tr -s ' ' | awk 'BEGIN {printf "#ifdef __GET_T2_LDR\n#undef
//__GET_T2_LDR\nunsigned __insn_t2_ldr[] = {\n"} {printf "  ARM::"$1",\n"}
//END  {printf "}\n#endif\n"}' > ../include/t2LDR.inc
//

using namespace llvm;

cl::opt<bool>
llvm::IskipEnableLoadStoreVerification("iskip-enable-load-store-verification",
                              cl::NotHidden,
                 cl::desc("Enable Load Store Verification Pass"),
                 cl::init(false));


namespace gen_inc {
#define __GET_T_LDR
#include "tLDR.inc"
#define __GET_T2_LDR
#include "t2LDR.inc"

#define __GET_T_STR
#include "tSTR.inc"
#define __GET_T2_STR
#include "t2STR.inc"
}


namespace {
char LoadStoreVerificationPass::ID = 0;

bool LoadStoreVerificationPass::extractInfo(MachineBasicBlock::iterator
                                            &MBBI, int &frameIdx,
                                            int64_t &immValue)
{
  {
    MachineOperand &destOperand = MBBI->getOperand(1);
    if (destOperand.isFI()) {
      frameIdx = destOperand.getIndex();
    }
    if (MBBI->getOperand(2).isImm()) {
      immValue = MBBI->getOperand(2).getImm();
    }
  }

  if (!MBBI->hasOneMemOperand()) {
    errs() << "XXX: more than one memory operand: " << *MBBI << "\n";
    return false;
  }

  if (MBBI->getOperand(1).isReg()) {
    errs() << "XXX: skip global: " << *MBBI << "\n";
    return false;
  }

  if (frameIdx == -1) {
    errs() << "XXX: object at unknown position on the stack: " << *MBBI << "\n";
    return false;
  }
  return true;
}

bool LoadStoreVerificationPass::verifyLoadInstruction(MachineBasicBlock &MBB,
                                           MachineBasicBlock::iterator
                                           &MBBI)
{
  if (!STI->isThumb2())
    return false;

  switch (MBBI->getDesc().getOpcode()) {
  default: return false;
  case ARM::t2LDRi12:
           break;
  }

  // TODO: assert instruction is idempotent

  int64_t immValue = 0;
  int frameIdx = 0;

  if (!extractInfo(MBBI, frameIdx, immValue))
    return false;

  //errs() << "XXX: " << *MBBI << " : " << MBBI->getOperand(1) << " frameIdx=" << frameIdx << ", immValue=" << immValue << "\n";

  return insertCheckLoadStore(MBB, MBBI, true, frameIdx, immValue);
}

bool LoadStoreVerificationPass::verifyStoreInstruction(MachineBasicBlock &MBB,
                                           MachineBasicBlock::iterator
                                           &MBBI)
{
  if (!STI->isThumb2())
    return false;

  switch (MBBI->getDesc().getOpcode()) {
  default: return false;
  case ARM::t2STRi12:
           break;
  }

  // TODO: assert instruction is idempotent

  int64_t immValue = 0;
  int frameIdx = 0;

  if (!extractInfo(MBBI, frameIdx, immValue))
    return false;

  //errs() << "XXX: " << *MBBI << " : " << MBBI->getOperand(1) << " frameIdx=" << frameIdx << ", immValue=" << immValue << "\n";

  return insertCheckLoadStore(MBB, MBBI, false, frameIdx, immValue);
}

bool LoadStoreVerificationPass::insertCheckLoadStore(MachineBasicBlock &MBB,
                                                MachineBasicBlock::iterator
                                                &MBBI,
                                                bool isLoad,
                                                int frameIdx, int64_t immValue)
{
  MachineMemOperand *origMO= *MBBI->memoperands_begin();
  MachineMemOperand *MyMO = origMO;

  DebugLoc DL = MBBI->getDebugLoc();

  MachineOperand &SourceRegOperand = MBBI->getOperand(0);
  assert(SourceRegOperand.isReg() && "source of store is not a reg?");
  unsigned SourceReg = SourceRegOperand.getReg();
  bool SourceRegIsKill = SourceRegOperand.isKill();

  if (ARMHardeningUtil::isInsideITBundle(MBBI))
    return false;

  unsigned SpareReg;
  if (!ARMHardeningUtil::getSpareRegister(TII, SourceReg, MBB, MBBI, SpareReg)) {
    if (ARMHardeningUtil::IskipEnableVerbose)
      errs() << "failed to get a spare reg for: " << *MBBI << "\n";
    return false;
  }

  MyMO->setFlags(isLoad ? MachineMemOperand::MOLoad : MachineMemOperand::MOStore);
  MachineBasicBlock *MyErrBB = getOrInsertErrorBasicBlock(MBB, MBBI);

  MachineBasicBlock::iterator lastInsn = MBBI;
  MachineBasicBlock::iterator firstInsn = MBBI;
  ++firstInsn;

  MachineBasicBlock *NewBB =
    ARMHardeningUtil::SplitMBBAt(TII,
                                 getAnalysisIfAvailable<MachineLoopInfo>(),
                                 MBB,
                                 firstInsn);
  if (!NewBB)
    return false;

  if (SourceRegIsKill) {
    SourceRegOperand.setIsKill(false);
  }

  ++lastInsn;
  AddDefaultPred(
    BuildMI(MBB, lastInsn, DL, TII->get(ARM::t2LDRi12))
    .addReg(SpareReg, RegState::Define)
    .addFrameIndex(frameIdx)
    .addMemOperand(MyMO)
    .addImm(immValue)
                );
  // TODO: save flags
  AddDefaultPred(
    BuildMI(MBB, lastInsn, DL, TII->get(ARM::t2CMPrr))
    .addReg(SpareReg, RegState::Kill)
    .addReg(SourceReg, SourceRegIsKill ? RegState::Kill : 0)
                );

  BuildMI(MBB, lastInsn, DL, TII->get(ARM::t2Bcc))
    .addMBB(MyErrBB)
    .addImm((int64_t)ARMCC::NE)
    .addReg(0);

  BuildMI(MBB, lastInsn, DL, TII->get(ARM::t2B))
    .addMBB(NewBB)
    .addImm((int64_t)ARMCC::AL)
    .addReg(0);

  if (!MBB.isSuccessor(MyErrBB))
    MBB.addSuccessorWithoutProb(MyErrBB);
  return true;
}

bool isCPSRdefAfterInsn(MachineBasicBlock &MBB,
                        MachineBasicBlock::iterator &MBBI
                       )
{
  // TODO: save flags only when necessary
  return true;
}


MachineBasicBlock *
LoadStoreVerificationPass::getOrInsertErrorBasicBlock(MachineBasicBlock
                                                      &MBB,
                                                      MachineBasicBlock::iterator
                                                      &MBBI)
{
  if (ErrBB != nullptr)
    return ErrBB;
  MachineFunction *MF = MBB.getParent();
  const BasicBlock *BB = MBB.getBasicBlock();
  ErrBB = MF->CreateMachineBasicBlock(BB);
  DebugLoc DL;// = MBBI->getDebugLoc();
  //BuildMI(ErrBB, DL, TII->get(ARM::t2UDF)).addImm(255 & (udfCnt++));
  BuildMI(ErrBB, DL, TII->get(ARM::t2UDF)).addImm(99);
  //BuildMI(ErrBB, DL, TII->get(ARM::t2UDF)).addImm(255 & (udfCnt++));
  // TODO: cannot insert b .
  //AddDefaultPred(
  //  BuildMI(ErrBB, DL, TII->get(ARM::t2B))
  //  .addMBB(ErrBB)
  //  );
  //ErrBB->addSuccessor(ErrBB);
  //MF.insert(MF.end(), ErrBB);
  //ARMFunctionInfo *AFI = MF->getInfo<ARMFunctionInfo>();
  //unsigned PCLabelId = AFI->createPICLabelUId();
  MF->push_back(ErrBB);
  if (ARMHardeningUtil::IskipEnableVerbose)
    errs() << "Inserting error handling BasicBlock\n";
  //AddDefaultPred(
  //  BuildMI(
  //    ErrBB,
  //    DL, TII->get(ARM::tBX_RET)));

  // This prevents ErrBB to be eliminated by the control flow optimizer
  // pass
  ErrBB->setIsEHPad();
  ErrBB->setIsEHFuncletEntry();
  //ErrBB->setHasAddressTaken();
  return ErrBB;
}

bool LoadStoreVerificationPass::isLoad(MachineBasicBlock::iterator
             &MBBI)
{
  // /usr/local/google/home/cojocar/code/llvm-dev/llvm-src/lib/Target/ARM/ARMInstrThumb.td
  const MCInstrDesc &desc = MBBI->getDesc();

#define check_in_array(a, opcode)                \
  do {                                           \
    unsigned idx;                                \
    for (idx = 0; idx < ARRAY_LEN(a); ++idx) {   \
      if (a[idx] == opcode)                      \
      return true;                               \
    }                                            \
  } while (0)

  check_in_array(gen_inc::__insn_t_ldr, desc.getOpcode());
  check_in_array(gen_inc::__insn_t2_ldr, desc.getOpcode());

  #if 0 // this is thumb mode
  switch (desc.getOpcode()) {
  // load/store
  case ARM::tLDRpci:
  case ARM::tLDRspi:
  case ARM::tLDRi:
  case ARM::tLDRr:
  case ARM::tLDRBi:
  case ARM::tLDRBr:
  case ARM::tLDRHi:
  case ARM::tLDRHr:
  case ARM::tLDRSB:
  case ARM::tLDRSH:
  // load/store multiple
  case ARM::tLDMIA:
  case ARM::tLDMIA_UPD:
    return true;
  // TODO: pop/store
  default:
    return false;
  };
  #endif

  return false;
}

bool LoadStoreVerificationPass::isStore(MachineBasicBlock::iterator
             &MBBI)
{
  const MCInstrDesc &desc = MBBI->getDesc();

  check_in_array(gen_inc::__insn_t_str, desc.getOpcode());
  check_in_array(gen_inc::__insn_t2_str, desc.getOpcode());

#if 0
  switch (desc.getOpcode()) {
  // load/store
  case ARM::tSTRspi:
  case ARM::tSTRi:
  case ARM::tSTRr:
  case ARM::tSTRBi:
  case ARM::tSTRBr:
  case ARM::tSTRHi:
  case ARM::tSTRHr:
  // load/store multiple
  case ARM::tSTMIA_UPD:
    return true;
  // TODO: pop/store
  default:
    return false;
  };
#endif

  return false;
}

bool LoadStoreVerificationPass::loadStoreVerificationBB(MachineBasicBlock &MBB)
{
  MachineBasicBlock::iterator MBBI;
  bool modified = false;

  if (MBB.begin() == MBB.end())
    return false;

  // do the loads
  for (MBBI = MBB.begin(); MBBI != MBB.end(); ++MBBI) {
    if (isInstructionIdempotent(MBBI)) {
      if (isLoad(MBBI)) {
        bool m = verifyLoadInstruction(MBB, MBBI);
        LoadsNotInstrumented += !m;
        modified |= m;
        if (m) {
          ++LoadStoresVerified;
          ++LoadStoreVerificationPass::MyLoadStoresVerified;
          // skip the rest of the basic block as we inserted a
          // terminator
          break;
        }
      }
    }
  }

  // do the stores
  for (MBBI = MBB.begin(); MBBI != MBB.end(); ++MBBI) {
    if (isInstructionIdempotent(MBBI)) {
      if (isStore(MBBI)) {
        bool m = verifyStoreInstruction(MBB, MBBI);
        StoresNotInstrumented += !m;
        modified |= m;
        if (m) {
          ++LoadStoresVerified;
          ++LoadStoreVerificationPass::MyLoadStoresVerified;
          // skip the rest of the basic block as we inserted a
          // terminator
          break;
        }
      }
    }
  }

  return modified;
}

bool
LoadStoreVerificationPass::isInstructionIdempotent(MachineBasicBlock::iterator
                                                    &MBBI)
{
  //errs() << "TODO: " << *MBBI << "\n";
  switch (MBBI->getDesc().getOpcode()) {
  case ARM::t2LDRB_PRE  :
  case ARM::t2LDRD_PRE  :
  case ARM::t2LDRH_PRE  :
  case ARM::t2LDRSB_PRE :
  case ARM::t2LDRSH_PRE :
  case ARM::t2LDR_PRE   :
  case ARM::t2STRB_PRE  :
  case ARM::t2STRD_PRE  :
  case ARM::t2STRH_PRE  :
  case ARM::t2STR_PRE   :
  case ARM::t2LDRB_POST :
  case ARM::t2LDRD_POST :
  case ARM::t2LDRH_POST :
  case ARM::t2LDRSB_POST:
  case ARM::t2LDRSH_POST:
  case ARM::t2LDR_POST  :
  case ARM::t2STRB_POST :
  case ARM::t2STRD_POST :
  case ARM::t2STRH_POST :
  case ARM::t2STR_POST  :
    return false;
  };
  return true;
}

bool LoadStoreVerificationPass::runOnMachineFunction(MachineFunction &Fn)
{
  bool modified = false;
  ErrBB = nullptr;

  if (!ARMHardeningUtil::shouldRunPassOnFunction(Fn))
    return false;

  STI = &static_cast<const ARMSubtarget &>(Fn.getSubtarget());
  TII = STI->getInstrInfo();
  TRI= Fn.getSubtarget().getRegisterInfo();

  if (Fn.getName() == "uart_rx_buf" ||
      Fn.getName() == "uart_tx_buf" ||
      Fn.getName() == "my_uart_tx_char" ||
      Fn.getName() == "my_uart_rx_char" ||
      Fn.getName() == "my_uart_init" ||
      Fn.getName() == "uart_tx_char" ||
      Fn.getName() == "uart_rx_char" ||
      Fn.getName() == "uart_init") {
    errs() << "blacklisted: " << Fn.getName() << "\n";
    return false;
  }

  if (ARMHardeningUtil::IskipEnableVerbose)
    errs() << "ARMHardening(external, load/store verification): " <<
    Fn.getName() << "\n";
  for (auto &bb : Fn) {
    modified |= loadStoreVerificationBB(bb);
  }

  if (ARMHardeningUtil::IskipEnableVerbose)
    errs() << LoadStoreVerificationPass::MyLoadStoresVerified <<
    " load/stores instrumented" << "\n";
  return modified;
}

}

FunctionPass *llvm::createLoadStoreVerificationPass() {
  return new LoadStoreVerificationPass();
}
