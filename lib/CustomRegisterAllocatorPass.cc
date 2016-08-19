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

#define DEBUG_TYPE "iskip-custom-regalloc"

#include "CustomRegisterAllocatorPass.h"

#include "util.h"

using namespace llvm;

STATISTIC(RejectedPhysRegs, "The # of regs rejected");

#define __include_external_regalloc
#include "../lib/RegAllocFast.cpp"
#undef __include_external_regalloc

namespace iskip {
class CustomRegisterAllocatorPass: public iskip_external::RAFast {
public:
  static char ID;
  CustomRegisterAllocatorPass() : iskip_external::RAFast() {}

  const char *getPassName() const override {
    return "ISKIP: Custom Register Allocator";
  }

public:
  LiveRegMap::iterator allocVirtReg(MachineInstr &MI,
                                    LiveRegMap::iterator LRI,
                                    unsigned Hint) override;
  LiveRegMap::iterator defineVirtReg(MachineInstr &MI,
                                     unsigned OpNum,
                                     unsigned VirtReg,
                                     unsigned Hint) override;
private:
  bool isPhysRegDstInInstr(unsigned PhysReg, MachineInstr &MI);
};
}

namespace iskip {
char CustomRegisterAllocatorPass::ID = 0;

iskip_external::RAFast::LiveRegMap::iterator
CustomRegisterAllocatorPass::allocVirtReg(MachineInstr &MI,
                                          LiveRegMap::iterator LRI,
                                          unsigned Hint) {
  if (MI.getParent() &&
     MI.getParent()->getParent() &&
     !ARMHardeningUtil::shouldRunPassOnFunction(*MI.getParent()->getParent())) {
    // apply the policy
    return iskip_external::RAFast::allocVirtReg(MI, LRI, Hint);
  }

  //DEBUG(dbgs() << "XXX: allocVirtReg, hint=" << Hint << "\n");
  //if (Hint && MI.getOpcode() == ARM::t2ADDrr) {
  //  llvm_unreachable("unreachable");
  //}
  //return RAFast::allocVirtReg(MI, LRI, Hint);
  const unsigned VirtReg = LRI->VirtReg;

  assert(TargetRegisterInfo::isVirtualRegister(VirtReg) &&
         "Can only allocate virtual registers");

  const TargetRegisterClass *RC = MRI->getRegClass(VirtReg);

  // Ignore invalid hints.
  if (Hint && (!TargetRegisterInfo::isPhysicalRegister(Hint) ||
               !RC->contains(Hint) || !MRI->isAllocatable(Hint)))
    Hint = 0;

  // Take hint when possible.
  if (Hint && !isPhysRegDstInInstr(Hint, MI)) {
    // Ignore the hint if we would have to spill a dirty register.
    unsigned Cost = calcSpillCost(Hint);
    if (Cost < spillDirty) {
      if (Cost)
        definePhysReg(MI, Hint, regFree);
      // definePhysReg may kill virtual registers and modify LiveVirtRegs.
      // That invalidates LRI, so run a new lookup for VirtReg.
      //DEBUG(dbgs() << "XXX: got cost/hint \n" << PrintReg(Hint, TRI) << "\n" );
      return assignVirtToPhysReg(VirtReg, Hint);
    }
  }

  ArrayRef<MCPhysReg> AO = RegClassInfo.getOrder(RC);

  // First try to find a completely free register.
  for (ArrayRef<MCPhysReg>::iterator I = AO.begin(), E = AO.end(); I != E; ++I){
    unsigned PhysReg = *I;
    if (PhysRegState[PhysReg] == regFree && !isRegUsedInInstr(PhysReg)) {
      if (!isPhysRegDstInInstr(PhysReg, MI)) {
        assignVirtToPhysReg(*LRI, PhysReg);
        //DEBUG(dbgs() << "XXX: got free phys reg\n" << PrintReg(PhysReg, TRI) << "\n" );
        return LRI;
      }
    }
  }

  DEBUG(dbgs() << "Allocating " << PrintReg(VirtReg) << " from "
               << TRI->getRegClassName(RC) << "\n");

  unsigned BestReg = 0, BestCost = spillImpossible;
  for (ArrayRef<MCPhysReg>::iterator I = AO.begin(), E = AO.end(); I != E; ++I){
    unsigned Cost = calcSpillCost(*I);
    DEBUG(dbgs() << "\tRegister: " << PrintReg(*I, TRI) << "\n");
    DEBUG(dbgs() << "\tCost: " << Cost << "\n");
    DEBUG(dbgs() << "\tBestCost: " << BestCost << "\n");
    if (isPhysRegDstInInstr(*I, MI)) continue;
    // Cost is 0 when all aliases are already disabled.
    if (Cost == 0) {
      assignVirtToPhysReg(*LRI, *I);
      return LRI;
    }
    if (Cost < BestCost)
      BestReg = *I, BestCost = Cost;
  }

  if (BestReg) {
    assert(!isPhysRegDstInInstr(BestReg, MI));
    definePhysReg(MI, BestReg, regFree);
    // definePhysReg may kill virtual registers and modify LiveVirtRegs.
    // That invalidates LRI, so run a new lookup for VirtReg.
    return assignVirtToPhysReg(VirtReg, BestReg);
  }

  // Nothing we can do. Report an error and keep going with a bad allocation.
  if (MI.isInlineAsm())
    MI.emitError("inline assembly requires more registers than available");
  else
    MI.emitError("ran out of registers during register allocation");
  // this is an error, we allocate the first register as virtual
  // register
  definePhysReg(MI, *AO.begin(), regFree);
  return assignVirtToPhysReg(VirtReg, *AO.begin());
}

bool
CustomRegisterAllocatorPass::isPhysRegDstInInstr(unsigned PhysReg, MachineInstr &MI)
{
  assert(TargetRegisterInfo::isPhysicalRegister(PhysReg) &&
         "Not a physical register");
  if (PhysReg == ARM::R10 || PhysReg == ARM::R12)
    return true;
  for (unsigned i = 0; i != MI.getNumOperands(); ++i) {
    MachineOperand &MO = MI.getOperand(i);
    //if (!MO.isReg() || !MO.isDef() || !MO.getReg() || MO.isEarlyClobber())
    //  continue;
    if (!MO.isReg() || !MO.getReg())
      continue;
    unsigned InUseReg = MO.getReg();
    if (TargetRegisterInfo::isPhysicalRegister(InUseReg)) {
      //DEBUG(dbgs() << "XXX: phys: " << PrintReg(MO.getReg(), TRI) << "\n");
      if (InUseReg == PhysReg) {
        ++RejectedPhysRegs;
        return true;
      }
    }
  }

  return false;
}

RAFast::LiveRegMap::iterator
CustomRegisterAllocatorPass::defineVirtReg(MachineInstr &MI, unsigned OpNum,
                                           unsigned VirtReg, unsigned Hint) {
  if (MI.getParent() &&
     MI.getParent()->getParent() &&
     !ARMHardeningUtil::shouldRunPassOnFunction(*MI.getParent()->getParent())) {
    // use the original function
    return iskip_external::RAFast::defineVirtReg(MI, OpNum, VirtReg, Hint);
  }

  assert(TargetRegisterInfo::isVirtualRegister(VirtReg) &&
         "Not a virtual register");
  LiveRegMap::iterator LRI;
  bool New;
  std::tie(LRI, New) = LiveVirtRegs.insert(LiveReg(VirtReg));
  if (New) {
    //if (MI.getOpcode() == ARM::t2ADDrr) {
    //  DEBUG(dbgs() << "XXX: hint=" << Hint << "\n");
    //}
    // If there is no hint, peek at the only use of this register.
    if ((!Hint || !TargetRegisterInfo::isPhysicalRegister(Hint)) &&
        MRI->hasOneNonDBGUse(VirtReg)) {
      const MachineInstr &UseMI = *MRI->use_instr_nodbg_begin(VirtReg);
      // It's a copy, use the destination register as a hint.
      if (UseMI.isCopyLike())
        Hint = UseMI.getOperand(0).getReg();
    }
    LRI = allocVirtReg(MI, LRI, Hint);
  } else if (LRI->LastUse) {
    // Redefining a live register - kill at the last use, unless it is this
    // instruction defining VirtReg multiple times.
    if (LRI->LastUse != &MI || LRI->LastUse->getOperand(LRI->LastOpNum).isUse())
      addKillFlag(*LRI);
  }
  assert(LRI->PhysReg && "Register not assigned");
  LRI->LastUse = &MI;
  LRI->LastOpNum = OpNum;
  LRI->Dirty = true;
  markRegUsedInInstr(LRI->PhysReg);
  return LRI;
}

}

FunctionPass *llvm::createCustomRegisterAllocatorPass()
{
  return new iskip::CustomRegisterAllocatorPass();
}
