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

#define DEBUG_TYPE "iskip-check-idempotent"

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

#include "CheckIdempotentPass.h"
#include "util.h"


STATISTIC(IdempotentInsn, "The # of OK idempotent insn");
STATISTIC(NotIdempotentInsn, "The # of NOT idempotent insn");
STATISTIC(CFIInstructions, "The # of CFI insns");

using namespace llvm;

cl::opt<bool>
llvm::IskipEnableCheckIdempotent("iskip-enable-check-idempotent",
                              cl::NotHidden,
                 cl::desc("Enable Instruction Check Idempotent Pass"),
                 cl::init(false));

cl::opt<bool>
llvm::IskipEnableCheckIdempotentCumulative("iskip-enable-check-idempotent-cumulative",
                              cl::NotHidden,
                 cl::desc("Enable Instruction Check Idempotent Pass (cumulative)"),
                 cl::init(true));


namespace {

static cl::opt<bool>
IskipVerboseCheckIdempotent("iskip-check-idempotent-verbose",
                              cl::Hidden,
                 cl::desc("Enable Instruction Duplication Pass"),
                 cl::init(false));
char CheckIdempotentPass::ID = 0;

bool CheckIdempotentPass::runOnMachineFunction(MachineFunction &Fn)
{
  bool modified = false;

  if (!ARMHardeningUtil::shouldRunPassOnFunction(Fn))
    return false;

  STI = &static_cast<const ARMSubtarget &>(Fn.getSubtarget());
  TII = STI->getInstrInfo();
  TRI = Fn.getSubtarget().getRegisterInfo();

  if (ARMHardeningUtil::IskipEnableVerbose)
    errs() << "ARMHardening(external, check idempotent insn): " <<
      Fn.getName() << "\n";
  for (auto &bb : Fn) {
    for (auto MBBI = bb.begin(); MBBI != bb.end(); ++MBBI) {
      if (MBBI->getDesc().getOpcode() == ARM::CFI_INSTRUCTION) {
        ++CFIInstructions;
      }
      if (ARMHardeningUtil::isInsnIdempotent(MBBI) &&
        !ARMHardeningUtil::isInsideITBundle(MBBI)) {
        ++IdempotentInsn;
      } else {
        if (IskipVerboseCheckIdempotent || ARMHardeningUtil::IskipEnableVerbose)
          errs() << "Not idempotent: " << PositionIdentifier << ": " <<  *MBBI << "\n";
        ++NotIdempotentInsn;
      }
    }
  }

  if (IskipVerboseCheckIdempotent)
    errs() << "idempotent=" << IdempotentInsn << "; notIdempotent=" <<
      NotIdempotentInsn << "\n";

  return false;
}

}

FunctionPass *llvm::createCheckIdempotentPass(const std::string MyMsg) {
  return new CheckIdempotentPass(MyMsg);
}
