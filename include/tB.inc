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

#ifdef __GET_TB
#undef __GET_TB
unsigned __insn_tb[] = {
  ARM::tB,
  ARM::tBIC,
  ARM::tBKPT,
  ARM::tBL,
  ARM::tBLXNSr,
  ARM::tBLXi,
  ARM::tBLXr,
  ARM::tBRIND,
  ARM::tBR_JTr,
  ARM::tBX,
  ARM::tBXNS,
  ARM::tBX_CALL,
  ARM::tBX_RET,
  ARM::tBX_RET_vararg,
  ARM::tBcc,
  ARM::tBfar,
};
#endif
