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

require 'tempfile'
require 'aruba/cucumber'


# test_src_dir the directory with all the test files *.ll
# llvm_dir is the llvm bin dir
Given(/^the bcfile of "(.*?)" as input$/) do |ll|
	bc=ll+'.bc'
	outbc=File.join(@current_output_dir, bc)
	ll_to_bc(File.join(@test_src_dir, ll), outbc)
	check_file_presence([outbc], true)
	@current_input_bc=outbc
end

When(/^llc runs with output "(.*?)" and with default flags$/) do |asm_out_file|
	out_asm =File.join(@current_output_dir, asm_out_file)
	check_file_presence([@current_input_bc], true)

	cmd=@llc + ' ' + @llc_def_flags + ' ' + @current_input_bc + ' -o ' + out_asm
	my_run_simple(unescape(cmd), true, 20)

	check_file_presence([out_asm], true)
	@current_output_asm.push(out_asm)
end

When(/^llc runs with output "(.*?)" and with passes$/) do |asm_out_file, table_of_passes|
	extra_flags_enabled =
		llc_get_passes_flags_as_str(get_array_from_table(table_of_passes))
	run_llc_with_extra_flags(asm_out_file, extra_flags_enabled)

	###cmd=@llc + ' ' + @llc_def_flags + ' ' + extra_flags_enabled + ' ' + @current_input_bc + ' -o ' + out_asm
	###my_run_simple(unescape(cmd), true, 20)

	###check_file_presence([out_asm], true)
	###@current_output_asm.push(out_asm)
end

Then(/^every asm should exist$/) do
	@current_output_asm.each do |out|
		if @verbose == true
			announce_or_puts("check " + out)
		end
		check_file_presence([out], true)
	end
end

Then(/^every asm output should be the same$/) do
	last_out=nil
	#@current_output_asm.push('/tmp/o')
	@current_output_asm.each do |out_asm|
		lines = get_lines_after_first_function(out_asm)
		if last_out.nil?
		else
			if last_out != lines
				fail("file differs")
			end
		end
		last_out = lines
	end
end

Then(/^the following command on every asm output should be true "(.*?)"$/) do |grep_cmd|
	@current_output_asm.each do |out_asm|
		run_grep_cmd_on_asm(out_asm, grep_cmd)
	end
end

Then(/^the following command on "(.*?)" asm should be true "(.*?)"$/) do |asm_out_file, grep_cmd|
	out_asm =File.join(@current_output_dir, asm_out_file)
	check_file_presence([out_asm], true)
	run_grep_cmd_on_asm(out_asm, grep_cmd)
end

####Given(/^an empty list of outputs$/) do
####	  pending # express the regexp above with the code you wish you had
####end
#
def run_grep_cmd_on_asm(asm_out_file, grep_cmd)
	grep_cmd_rep = grep_cmd.sub('%out%', asm_out_file)
	my_run_simple('bash -c "' + grep_cmd_rep + '"', true, 20)
end

def get_array_from_table(table)
	ret=Array.new
	table.raw.each do |c|
		ret.push(c[0])
	end
	ret
end

def llc_get_passes_flags_as_str(passes_enabled)
	extra_flags_enabled = ''
	passes_enabled.each do |pass|
		extra_flags_enabled = extra_flags_enabled +'-iskip-enable-' + pass + ' '
	end
	extra_flags_enabled
end

def clang_get_passes_flags_as_str(passes_enabled)
	extra_flags_enabled = ''
	passes_enabled.each do |pass|
		extra_flags_enabled = extra_flags_enabled +'-mllvm -iskip-enable-' + pass + ' '
	end
	extra_flags_enabled
end

def get_flags_as_str(flags)
	extra_flags = ''
	flags.each do |flag|
		extra_flags += '-'+flag + ' '
	end
	extra_flags
end


def run_llc_with_extra_flags(asm_out_file, flags_str)
	out_asm =File.join(@current_output_dir, asm_out_file)
	check_file_presence([@current_input_bc], true)

	cmd=@llc + ' ' + @llc_def_flags + ' ' + flags_str + ' ' + @current_input_bc + ' -o ' + out_asm
	my_run_simple(unescape(cmd), true, 20)

	check_file_presence([out_asm], true)
	@current_output_asm.push(out_asm)
end

Given(/^a temporary directory output$/) do
	@current_output_dir=Dir.mktmpdir('iskip-llvm-tests-')
	if @verbose == true
		announce_or_puts("current output is in " + @current_output_dir)
	end
	@current_output_asm=Array.new
	@current_output_obj=Array.new
	@current_input_bc=nil
	@current_input_c=nil
end

When(/^llc runs with output "(.*?)" and with extra flag "(.*?)"$/) do |asm_out_file, flag|
	flags = Array.new
	flags.push(flag)

	run_llc_with_extra_flags(asm_out_file, get_flags_as_str(flags))
end

When(/^llc runs with output "(.*?)" and with extra flags$/) do |asm_out_file, table_of_flags|
	run_llc_with_extra_flags(asm_out_file,
							 get_flags_as_str(
								 get_array_from_table(table_of_flags)))
end

Given(/^the C file "(.*?)" as input$/) do |c_input|
	c_input_full_path =File.join(@test_src_dir, c_input)
	check_file_presence([c_input_full_path], true)
	@current_input_c=c_input_full_path
end

When(/^clang runs with output "(.*?)" and with default flags$/) do |out_obj_file|
	out_obj = File.join(@current_output_obj, out_obj_file)

	run_clang_with_extra_flags(out_obj, "")
end

When(/^clang runs with output "(.*?)" and with passes$/) do |out_obj_file, table_of_passes|
	extra_flags_enabled =
		clang_get_passes_flags_as_str(get_array_from_table(table_of_passes))
	run_clang_with_extra_flags(out_obj_file, extra_flags_enabled)
end
	
Then(/^every object should exist$/) do
	@current_output_obj.each do |out|
		if @verbose == true
			announce_or_puts("check " + out)
		end
		check_file_presence([out], true)
	end
end

When(/^clang runs with output "(.*?)" and with extra clang flag "(.*?)"$/) do |out_obj_file, flag|
	flags = Array.new
	flags.push(flag)

	run_clang_with_extra_flags(out_obj_file, get_flags_as_str(flags))
end

When(/^clang runs with output "(.*?)" and with extra clang flags$/) do |out_obj_file, table_of_flags|
	run_clang_with_extra_flags(out_obj_file, get_flags_as_str(
		get_array_from_table(table_of_flags)))
end

def run_clang_with_extra_flags(out_obj_file, flags_str)
	out_obj = File.join(@current_output_dir, out_obj_file)
	check_file_presence([@current_input_c], true)

	cmd=@clang + ' ' + @clang_def_flags + ' ' + flags_str + ' ' + @current_input_c + ' -c -o ' + out_obj
	my_run_simple(unescape(cmd), true, 20)

	check_file_presence([out_obj], true)
	@current_output_obj.push(out_obj)
end

def my_run_simple(cmd, tf, t)
	if @verbose == true
		announce_or_puts("run: " + cmd)
	end
	run_simple(cmd, tf, t)
end

def ll_to_bc(filein, fileout)
	check_file_presence([filein], true)
	cmd=@llvm_as + " " + filein + " -o " + fileout
	my_run_simple(unescape(cmd), true, 20)
end

def get_lines_after_first_function(filein)
	isInsideFunction=false
	lines=Array.new
	File.open(filein, "r").each_line do |line|
		m = (/^[^.@]{1}[^:]*:$/ =~ line)
		unless m.nil?
			# got a new function
			isInsideFunction=true
		end
		if isInsideFunction == true
			lines.push(line)
		end
	end
	#announce_or_puts(lines)
	lines
end


###def get_functions_from_asm_file(filein)
###	functions=Array.new
###	lines_of_function=Array.new
###	isInsideFunction=false
###	File.open(filein, "r").each_line do |line|
###		m = (/^[^.@]{1}[^:]*:$/ =~ line)
###		unless m.nil?
###			# got a new function
###			if isInsideFunction
###		end
###	end
###end

