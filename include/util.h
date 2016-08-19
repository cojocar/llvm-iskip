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

#ifndef __UTIL_H__
#define __UTIL_H__ 1

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

#ifndef ARRAY_LEN
#define ARRAY_LEN(a) (sizeof (a) / sizeof (a[0]))
#endif

namespace ARMHardeningUtil {
  // add attribute__((annotate("my_annotation"))); in C code
  bool hasAnnotation(llvm::MachineFunction &MF, std::string name);
  // return true if we can get a spare reg
  bool getSpareRegister(
    const llvm::TargetInstrInfo *TII,
    unsigned SourceReg, llvm::MachineBasicBlock &MBB,
    llvm::MachineBasicBlock::iterator &MBBI,
    unsigned &SpareReg,
    bool shouldFailIfCpsrIsLive = true,
    bool beforeInsn = true);

  // BBI1 will be the first instruction in the new basic block
  llvm::MachineBasicBlock *SplitMBBAt(
    const llvm::TargetInstrInfo *TII,
    llvm::MachineLoopInfo *MLI,
    llvm::MachineBasicBlock &CurMBB,
    llvm::MachineBasicBlock::iterator &BBI1);

  void computeLiveIns(const llvm::TargetRegisterInfo *TRI,
                      llvm::MachineBasicBlock &MBB);
  bool isInsnIdempotent(llvm::MachineBasicBlock::iterator &MBBI);
  bool isInsideITBundle(llvm::MachineBasicBlock::iterator &MBBI);

  extern llvm::cl::opt<bool> IskipEnableVerbose;

  enum IskipDeployPolicyEnum {
    IskipDeploy_any_function,         // deploy the passes on every function
    IskipDeploy_enforce_whitelist,    // deploy only on whitelisted functions
    IskipDeploy_enforce_blacklist,    // deploy only on non-blacklisted function
  };

  extern llvm::cl::opt<IskipDeployPolicyEnum> IskipDeployPolicy;

  bool shouldRunPassOnFunction(llvm::MachineFunction &MF);
}

#endif
