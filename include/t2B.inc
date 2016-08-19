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

#ifdef __GET_T2B
#undef __GET_T2B
unsigned __insn_t2b[] = {
  ARM::t2B,
  ARM::t2BFC,
  ARM::t2BFI,
  ARM::t2BICri,
  ARM::t2BICrr,
  ARM::t2BICrs,
  ARM::t2BR_JT,
  ARM::t2BXJ,
  ARM::t2Bcc,
};
#endif
