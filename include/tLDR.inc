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

#ifdef __GET_T_LDR
#undef __GET_T_LDR
unsigned __insn_t_ldr[] = {
  ARM::tLDRBi,
  ARM::tLDRBr,
  ARM::tLDRConstPool,
  ARM::tLDRHi,
  ARM::tLDRHr,
  ARM::tLDRLIT_ga_abs,
  ARM::tLDRLIT_ga_pcrel,
  ARM::tLDRSB,
  ARM::tLDRSH,
  ARM::tLDR_postidx,
  ARM::tLDRi,
  ARM::tLDRpci,
  ARM::tLDRpci_pic,
  ARM::tLDRr,
  ARM::tLDRspi,
};
#endif
