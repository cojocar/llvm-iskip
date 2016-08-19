#!/bin/bash
#
# Copyright 2016 The Iskip Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################

set -x
tp=third_party
# compile LLVM
# make sure that you have cmake at least version 3.6.0 in your path
mkdir -p ${tp}/llvm-{build,install}

llvm_install_path=`realpath ${tp}/llvm-install`
llvm_build_path=`realpath ${tp}/llvm-build`
llvm_src_path=`realpath ${tp}/llvm`
njobs=`getconf _NPROCESSORS_ONLN`

(pushd "${llvm_build_path}" &&
  cmake -G "Unix Makefiles" "${llvm_src_path}" -DCMAKE_INSTALL_PREFIX="${llvm_install_path}" &&
  make -j${njobs} install &&
  popd)

# install symlinks
test -L "${llvm_build_path}" || ln -s "${llvm_build_path}" include/llvm-build
test -L "${llvm_src_path}" || ln -s "${llvm_src_path}" include/llvm-src

# compile passes
test -d build || mkdir -p build
(pushd build &&
  LLVM_BUILD_DIR="${llvm_build_path}" LLVM_DIR="${llvm_build_path}" cmake .. &&
  LLVM_BUILD_DIR="${llvm_build_path}" make -j${njobs} &&
  popd)
