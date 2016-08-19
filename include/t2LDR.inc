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

#ifdef __GET_T2_LDR
#undef __GET_T2_LDR
unsigned __insn_t2_ldr[] = {
  ARM::t2LDRBT,
  ARM::t2LDRB_POST,
  ARM::t2LDRB_PRE,
  ARM::t2LDRBi12,
  ARM::t2LDRBi8,
  ARM::t2LDRBpci,
  ARM::t2LDRBpcrel,
  ARM::t2LDRBs,
  ARM::t2LDRConstPool,
  ARM::t2LDRD_POST,
  ARM::t2LDRD_PRE,
  ARM::t2LDRDi8,
  ARM::t2LDREX,
  ARM::t2LDREXB,
  ARM::t2LDREXD,
  ARM::t2LDREXH,
  ARM::t2LDRHT,
  ARM::t2LDRH_POST,
  ARM::t2LDRH_PRE,
  ARM::t2LDRHi12,
  ARM::t2LDRHi8,
  ARM::t2LDRHpci,
  ARM::t2LDRHpcrel,
  ARM::t2LDRHs,
  ARM::t2LDRSBT,
  ARM::t2LDRSB_POST,
  ARM::t2LDRSB_PRE,
  ARM::t2LDRSBi12,
  ARM::t2LDRSBi8,
  ARM::t2LDRSBpci,
  ARM::t2LDRSBpcrel,
  ARM::t2LDRSBs,
  ARM::t2LDRSHT,
  ARM::t2LDRSH_POST,
  ARM::t2LDRSH_PRE,
  ARM::t2LDRSHi12,
  ARM::t2LDRSHi8,
  ARM::t2LDRSHpci,
  ARM::t2LDRSHpcrel,
  ARM::t2LDRSHs,
  ARM::t2LDRT,
  ARM::t2LDR_POST,
  ARM::t2LDR_PRE,
  ARM::t2LDRi12,
  ARM::t2LDRi8,
  ARM::t2LDRpci,
  ARM::t2LDRpci_pic,
  ARM::t2LDRpcrel,
  ARM::t2LDRs,
};
#endif
