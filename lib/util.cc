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

#include "util.h"

using namespace llvm;

cl::opt<ARMHardeningUtil::IskipDeployPolicyEnum>
ARMHardeningUtil::IskipDeployPolicy("iskip-deploy-policy",
                                           cl::NotHidden,
          cl::desc("Specify when to deploy the passes."),
          cl::values(
            clEnumValN(ARMHardeningUtil::IskipDeploy_any_function,
                       "any", "Deploy on every function regardless of "
                       "annotation. This is the default"),
            clEnumValN(ARMHardeningUtil::IskipDeploy_enforce_whitelist,
                       "whitelist" , "Apply the passes only on the "
                       "whitelist. Do not apply the passes on other functions"),
            clEnumValN(ARMHardeningUtil::IskipDeploy_enforce_blacklist,
                       "blacklist" , "Do not apply the passes on the "
                       "blacklist. Apply the passes on other functions."),
            clEnumValEnd),
          cl::init(ARMHardeningUtil::IskipDeploy_any_function));

namespace ARMHardeningUtil {
  static const std::string enableAnnotation = "armhardnening=true";
  static const std::string disableAnnotation = "armhardnening=false";
  bool hasAnnotation(MachineFunction &MF, std::string name)
  {
    const Function *F = MF.getFunction();
    if (F == nullptr)
      return false;

    auto M = F->getParent();
    if (nullptr == M)
      return false;
    auto GOB = M->getNamedGlobal("llvm.global.annotations");
    if (!GOB)
      return false;

    auto a = cast<ConstantArray>(GOB->getOperand(0));
    for (unsigned i = 0; i < a->getNumOperands(); i++) {
      auto e = cast<ConstantStruct>(a->getOperand(i));

      if (auto fn = dyn_cast<Function>(e->getOperand(0)->getOperand(0))) {
        auto anno =
          cast<ConstantDataArray>(
            cast<GlobalVariable>(
              e->getOperand(1)->getOperand(0))->getOperand(0))->getAsCString();
        if (anno == name)
          return true;
      }
    }

    return false;
  }

  // get a spare register right *before* MBBI is executed
  bool getSpareRegister(
    const llvm::TargetInstrInfo *TII,
    unsigned SourceReg,
    MachineBasicBlock &MBB,
    MachineBasicBlock::iterator &MBBI,
    unsigned &SpareReg,
    bool shouldFailIfCpsrIsLive,
    bool beforeInsn)
  {
    static unsigned Candidates[] = {
      ARM::R10, ARM::R0, ARM::R1, ARM::R2, ARM::R3, ARM::R4, ARM::R5,
      ARM::R9, ARM::R11, ARM::R12, ARM::LR,
    };
    //bool ret = false;
    LivePhysRegs LiveRegs(&((const ARMBaseInstrInfo *)TII)->getRegisterInfo());

    LiveRegs.addLiveIns(MBB);
    LiveRegs.addLiveOuts(MBB);
    LiveRegs.removeReg(ARM::R10);

    const MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();
#if 1
    for (auto I = std::prev(MBB.end()); I != MBBI; --I) {
     // errs() << "before: " << *I << "" << LiveRegs << "";
      LiveRegs.stepBackward(*I);
      //errs() << "after: " << *I << "" << LiveRegs << "";
    }
    // *before* MBBI is executed
    //errs() << "before(E): " << *MBBI << "" << LiveRegs << "";
    if (beforeInsn) {
      LiveRegs.stepBackward(*MBBI);
    }
    //errs() << "after(E): " << *MBBI << "" << LiveRegs << "";
    //errs() << MBB;
    //
#endif

    if (shouldFailIfCpsrIsLive && !LiveRegs.available(MRI, ARM::CPSR)) {
      errs() << "XXX: CPSR live, skiping\n";
      return false;
    }

    for (unsigned CandidateReg : Candidates) {
      if (SourceReg == CandidateReg)
        continue;
      if (LiveRegs.available(MRI, CandidateReg)) {
        SpareReg = CandidateReg;
        return true;
      }
    }
    return false;
  }

  MachineBasicBlock *SplitMBBAt(
    const llvm::TargetInstrInfo *TII,
    llvm::MachineLoopInfo *MLI,
    MachineBasicBlock &CurMBB,
    MachineBasicBlock::iterator &BBI1)
  {
    if (BBI1 ==CurMBB.end() || !TII->isLegalToSplitMBBAt(CurMBB, BBI1)) {
      if (BBI1 == CurMBB.end())
        errs() << "Failed to split, cannot split at end\n";
      else
        errs() << "Failed to split, illegal split\n";
      return nullptr;
    }
    MachineFunction &MF = *CurMBB.getParent();

    // Create the fall-through block.
    MachineFunction::iterator MBBI = CurMBB.getIterator();
    const BasicBlock *BB = CurMBB.getBasicBlock();
    if (!BB) {
      errs() << "Failed to split, missing BB\n";
      return nullptr;
    }
    MachineBasicBlock *NewMBB =MF.CreateMachineBasicBlock(BB);

    // insert the new bb after the curMBB
    CurMBB.getParent()->insert(++MBBI, NewMBB);

    // Move all the successors of this block to the specified block.
    NewMBB->transferSuccessors(&CurMBB);

    // Add an edge from CurMBB to NewMBB for the fall-through.
    //CurMBB.addSuccessor(NewMBB);
    CurMBB.addSuccessor(NewMBB, BranchProbability::getOne());

    // Splice the code over.
    NewMBB->splice(NewMBB->end(), &CurMBB, BBI1, CurMBB.end());

    // NewMBB belongs to the same loop as CurMBB.
    //MachineLoopInfo *MLI = getAnalysisIfAvailable<MachineLoopInfo>();
    if (MLI)
      if (MachineLoop *ML = MLI->getLoopFor(&CurMBB))
        ML->addBasicBlockToLoop(NewMBB, MLI->getBase());

    // NewMBB inherits CurMBB's block frequency.
    //MBBFreqInfo.setBlockFreq(NewMBB, MBBFreqInfo.getBlockFreq(&CurMBB));

    auto Fn = CurMBB.getParent();
    assert(Fn != nullptr && "Cannot split an orphan BB");
    const TargetRegisterInfo *TRI = Fn->getSubtarget().getRegisterInfo();
    assert(TRI != nullptr && "cannot compute LiveIns with no register info");
    ARMHardeningUtil::computeLiveIns(TRI, *NewMBB);

    return NewMBB;
  }

  void computeLiveIns(const llvm::TargetRegisterInfo *TRI, MachineBasicBlock &MBB)
  {
    LivePhysRegs LiveRegs;

    LiveRegs.init(TRI);
    LiveRegs.addLiveOutsNoPristines(MBB);
    for (MachineInstr &MI : make_range(MBB.rbegin(), MBB.rend()))
      LiveRegs.stepBackward(MI);

    for (unsigned Reg : LiveRegs) {
      // Skip the register if we are about to add one of its super registers.
      bool ContainsSuperReg = false;
      for (MCSuperRegIterator SReg(Reg, TRI); SReg.isValid(); ++SReg) {
        if (LiveRegs.contains(*SReg)) {
          ContainsSuperReg = true;
          break;
        }
      }
      if (ContainsSuperReg)
        continue;
      MBB.addLiveIn(Reg);
    }
  }

  bool isInsnIdempotent(MachineBasicBlock::iterator &MBBI) {
    const MachineInstr &MI = *MBBI;
    auto itUses = MI.uses();
    auto itDefs = MI.defs();
    SmallSetVector<unsigned, 4> uses;
    SmallSetVector<unsigned, 4> defs;
    SmallSetVector<unsigned, 4> regOps;

    switch (MBBI->getDesc().getOpcode()) {
    default: break;
    case ARM::tBLXr:
    case ARM::tBL:
    case ARM::INLINEASM:
             return false;
    }

    // search if we have the operand twice, too rigid
    for (unsigned idx = 0; idx < MBBI->getNumOperands(); ++idx) {
      MachineOperand &MO = MBBI->getOperand(idx);
      if (MO.isReg() && MO.getReg()) {
        if (regOps.count(MO.getReg()))
            return false;
        regOps.insert(MO.getReg());
      }
    }

    unsigned opCode = MBBI->getDesc().getOpcode();
    if (opCode == ARM::t2LDMIA ||
        opCode == ARM::tLDMIA ||
        opCode == ARM::t2LDMDB) {
      MachineOperand &MO = MBBI->getOperand(0);
      if (MO.isReg() && MO.getReg() &&
          TargetRegisterInfo::isPhysicalRegister(MO.getReg())) {
        uses.insert(MO.getReg());
      }
    }



    for (auto it = itUses.begin() ; it != itUses.end(); ++it) {
      const MachineOperand MO = *it;
      if (!MO.isReg() || !MO.getReg())
        continue;
      uses.insert(MO.getReg());
    }
    for (auto it = itDefs.begin() ; it != itDefs.end(); ++it) {
      const MachineOperand MO = *it;
      if (!MO.isReg() || !MO.getReg())
        continue;
      defs.insert(MO.getReg());
    }

    for (auto u : uses)
      if (defs.count(u))
        return false ;

    return true;
  }

  bool isInsideITBundle(MachineBasicBlock::iterator &MBBI) {
    for (unsigned idx = 0; idx < MBBI->getNumOperands(); ++idx) {
      MachineOperand &MO = MBBI->getOperand(idx);
      if (MO.isReg() && MO.getReg() == ARM::ITSTATE)
        return true;
    }
    return false;
  }

  bool shouldRunPassOnFunction(MachineFunction &MF) {

    switch (IskipDeployPolicy) {
    case IskipDeploy_any_function:
      return true;
    case IskipDeploy_enforce_whitelist:
      return hasAnnotation(MF, enableAnnotation);
    case IskipDeploy_enforce_blacklist:
      return !hasAnnotation(MF, disableAnnotation);
    default:
      llvm_unreachable("Invalid policy selection");
    };

    return false;
  }
}
