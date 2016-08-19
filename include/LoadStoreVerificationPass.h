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

#ifndef __LOADSTORE_VERIFICATION_PASS_H__
#define __LOADSTORE_VERIFICATION_PASS_H__ 1

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegisterClassInfo.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetRegisterInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Pass.h"
#include "llvm/MC/MCInst.h"
#include "llvm/ADT/Statistic.h"
#include "llvm-src/include/llvm/CodeGen/MachineConstantPool.h"
#include "llvm-src/lib/Target/ARM/ARMMachineFunctionInfo.h"
#include "llvm-src/lib/Target/ARM/ARMConstantPoolValue.h"

//#include "llvm-build/lib/Target/ARM/ARMSubtarget.h"
#include "llvm-src/lib/Target/ARM/ARMSubtarget.h"

namespace {
struct LoadStoreVerificationPass: public llvm::MachineFunctionPass {
  static char ID;
  LoadStoreVerificationPass() : MachineFunctionPass(ID) {}

  llvm::MachineFunctionProperties getRequiredProperties() const override {
    return llvm::MachineFunctionProperties().set(
      llvm::MachineFunctionProperties::Property::AllVRegsAllocated);
  }

  const char *getPassName() const override {
    return "ISKIP: LoadStore Verification Pass";
  }

  bool runOnMachineFunction(llvm::MachineFunction &Fn) override;

private:
  bool loadStoreVerificationBB(llvm::MachineBasicBlock &MBB);
  const llvm::TargetInstrInfo *TII;
  const llvm::TargetRegisterInfo *TRI;
  const llvm::ARMSubtarget *STI;
  bool isInstructionIdempotent(llvm::MachineBasicBlock::iterator &MBBI);
  bool isStore(llvm::MachineBasicBlock::iterator &MBBI);
  bool isLoad(llvm::MachineBasicBlock::iterator &MBBI);
  void computeLiveIns(llvm::MachineBasicBlock &MBB);
  bool extractInfo(llvm::MachineBasicBlock::iterator &MBBI, int
                   &frameIdx, int64_t &immValue);
  bool insertCheckLoadStore(llvm::MachineBasicBlock &MBB,
                       llvm::MachineBasicBlock::iterator &MBBI,
                       bool isLoad,
                       int frameIdx, int64_t immValue);

  unsigned MyLoadStoresVerified = 0;
  unsigned udfCnt = 0;

  bool verifyLoadInstruction(llvm::MachineBasicBlock &MBB,
                 llvm::MachineBasicBlock::iterator &MBBI);
  bool verifyStoreInstruction(llvm::MachineBasicBlock &MBB,
                 llvm::MachineBasicBlock::iterator &MBBI);

  llvm::MachineBasicBlock *ErrBB = nullptr;
  llvm::MachineBasicBlock *getOrInsertErrorBasicBlock(llvm::MachineBasicBlock &MF,
                                                      llvm::MachineBasicBlock::iterator
                                                      &MBBI);
  bool isCPSRdefAfterInsn(llvm::MachineBasicBlock &MBB,
                          llvm::MachineBasicBlock::iterator &MBBI
                         );
};
}

namespace llvm {
  extern cl::opt<bool> IskipEnableLoadStoreVerification;
  llvm::FunctionPass *createLoadStoreVerificationPass();
}

#endif
