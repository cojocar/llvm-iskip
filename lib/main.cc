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

#include "llvm/Pass.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

#include "InstructionDuplicationPass.h"
#include "NopInsertionPass.h"
#include "CallInsertionPass.h"
#include "LoadStoreVerificationPass.h"
#include "BranchDuplicationPass.h"
#include "CustomRegisterAllocatorPass.h"
#include "BLReplacePass.h"
#include "PushPopReplacePass.h"
#include "ITReplacePass.h"
#include "LSMUpdateReplacePass.h"
#include "CheckIdempotentPass.h"

#include <vector>

#include "util.h"

using namespace llvm;

cl::opt<bool>
IskipEnableAllInstructionDuplicationPasses("iskip-enable-all-duplication-passes",
                                           cl::NotHidden,
                 cl::desc("Enable all passes to achieve ins duplication."),
                 cl::init(true));
cl::opt<bool>
ARMHardeningUtil::IskipEnableVerbose("iskip-verbose",
                                           cl::NotHidden,
                 cl::desc("Print debug messages inside the."),
                 cl::init(false));

namespace llvm {
  std::vector<FunctionPass *> createTargetARMCustomRegAllocPasses() {
    std::vector<FunctionPass *> allPasses;

    // the custom register allocator is disabled by
    // -optimize-regalloc=false -mllvm -use-external-regalloc=false
    // and it is enabled when
    // -optimize-regalloc=false -mllvm -use-external-regalloc=true
    allPasses.emplace_back(llvm::createCustomRegisterAllocatorPass());

    if (ARMHardeningUtil::IskipEnableVerbose)
      errs() << "createTargetARMCustomRegAllocPasses\n";

    return allPasses;
  }

  std::vector<FunctionPass *> createARMHardeningPassLibPostRegAlloc() {
    std::vector<FunctionPass *> allPasses;

    if (ARMHardeningUtil::IskipEnableVerbose)
      errs() << "createARMHardeningPassLibPostRegAlloc\n";

    if (IskipEnableLoadStoreVerification)
      allPasses.emplace_back(llvm::createLoadStoreVerificationPass());

    if (IskipEnableAllInstructionDuplicationPasses ||
        IskipEnableLSMUpdateReplace)
      allPasses.emplace_back(llvm::createLSMUpdateReplacePass());

    // The BLReplace pass uses an insn that has to be lowered by the
    // thumb2lowering pass. We should run this pass before
    // Thumb2SizeReduction
    if (IskipEnableAllInstructionDuplicationPasses || IskipEnableBLReplace)
      allPasses.emplace_back(llvm::createBLReplacePass());

    if (IskipEnableCheckIdempotent && IskipEnableCheckIdempotentCumulative)
      allPasses.emplace_back(llvm::createCheckIdempotentPass("(PostRegAlloc)"));

    return allPasses;
  }

  std::vector<FunctionPass *> createARMHardeningPassLibPostPreSched2() {
    std::vector<FunctionPass *> allPasses;

    if (IskipEnableAllInstructionDuplicationPasses ||
        IskipEnableLSMUpdateReplace)
      allPasses.emplace_back(llvm::createLSMUpdateReplacePass());

    if (IskipEnableCheckIdempotent && IskipEnableCheckIdempotentCumulative)
      allPasses.emplace_back(llvm::createCheckIdempotentPass("(PostPreSched2)"));

    if (ARMHardeningUtil::IskipEnableVerbose)
      errs() << "createARMHardeningPassLibPostPreSched2\n";

    return allPasses;
  }

  std::vector<FunctionPass *> createARMHardeningPassLibPreConstantIsland() {
    std::vector<FunctionPass *> allPasses;

    if (IskipEnableAllInstructionDuplicationPasses ||
        IskipEnableITReplace)
      allPasses.emplace_back(llvm::createITReplacePass());

    if (IskipEnableAllInstructionDuplicationPasses ||
        IskipEnableLSMUpdateReplace)
      allPasses.emplace_back(llvm::createLSMUpdateReplacePass());

    if (IskipEnableAllInstructionDuplicationPasses ||
        IskipEnablePushPopReplace)
      allPasses.emplace_back(llvm::createPushPopReplacePass());

    if (IskipEnableCheckIdempotent)
      allPasses.emplace_back(llvm::createCheckIdempotentPass("(BeforeInsnDuplication)"));

    if (IskipEnableAllInstructionDuplicationPasses ||
        IskipEnableInstructionDuplication)
      allPasses.emplace_back(llvm::createInstructionDuplicationPass());

    if (IskipEnableCallInsertion != IskipCallInsertion::Insn_none)
      allPasses.emplace_back(llvm::createCallInsertionPass());

    if (IskipEnableBranchDuplication)
      allPasses.emplace_back(llvm::createBranchDuplicationPass());

    if (IskipEnableNopInsertion)
      allPasses.emplace_back(createNopInsertionPass());

    if (ARMHardeningUtil::IskipEnableVerbose)
      errs() << "createARMHardeningPassPreConstantIsland\n";

    return allPasses;
  }

  std::vector<FunctionPass *> createARMHardeningPassLibPostPreEmit() {
    std::vector<FunctionPass *> allPasses;

    //if (IskipEnableCheckIdempotent)
    //  allPasses.emplace_back(llvm::createCheckIdempotentPass("(PostPreEmit)"));

    if (ARMHardeningUtil::IskipEnableVerbose)
      errs() << "createARMHardeningPassPostPreEmit\n";

    return allPasses;
  }

}
