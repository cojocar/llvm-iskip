#!/usr/bin/ruby
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

require 'aruba/cucumber'

Before do
	if ENV.key?('LLVM_INSTALL_DIR')
		llvm_dir = ENV['LLVM_INSTALL_DIR']
	else
		llvm_dir = "../third_party/llvm-install/bin/"
	end
	@llvm_dir = File.absolute_path(llvm_dir)

	@llvm_as = File.absolute_path(File.join(@llvm_dir, "llvm-as"))
	@llc = File.absolute_path(File.join(@llvm_dir, "llc"))
	@clang = File.absolute_path(File.join(@llvm_dir, "clang"))
	@llvm_dis = File.absolute_path(File.join(@llvm_dir, "llvm-dis"))

	if ENV.key?('LLVM_ISKIP_BUILD_DIR')
		so_path =
			File.join(ENV['LLVM_ISKIP_BUILD_DIR'],
					  "lib",
					  "LLVMCodeGenHardnening.so")
	else
		so_path =
			File.join(File.dirname(__FILE__), "..", "..",
					  "..", "build", "lib",
					  "LLVMCodeGenHardnening.so")
	end
	so_path=File.absolute_path(so_path)

	@test_src_dir=File.absolute_path(File.join(File.dirname(__FILE__), "..", ".."))

	@llc_def_flags = "-optimize-regalloc=false "\
		"-use-external-regalloc=true "\
		"-arm-force-fast-isel=false " \
		"-march=thumb "\
		"-mcpu=cortex-m3 "\
		"-float-abi=soft "\
		"-arm-implicit-it=never "\
		"-load=" + so_path

	@clang_def_flags = "-mllvm -optimize-regalloc=false " \
		"-mllvm -use-external-regalloc=true "\
		"-mllvm -arm-implicit-it=never "\
		"-Xclang -load -Xclang " + so_path + " " \
		"-target arm-none-eabi -mcpu=cortex-m3 -mfloat-abi=soft -mthumb "

	@verbose=false
	if ENV.key?('V')
		if '1' == ENV['V']
			@verbose=true
		end
	end

	#announce_or_puts("llvm_dir = " + @llvm_dir)
	#announce_or_puts("test_src_dir = " + @test_src_dir)
	#announce_or_puts(ENV.inspect)
end
