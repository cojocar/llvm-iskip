Feature: Check that clang generates some code when running with opts

	Background:
		Given a temporary directory output
		Given the C file "empty-func.c" as input

		When clang runs with output "out-O0.o" and with extra clang flag "O0"
		When clang runs with output "out-O1.o" and with extra clang flag "O1"
		When clang runs with output "out-O2.o" and with extra clang flag "O2"
		When clang runs with output "out-O3.o" and with extra clang flag "O3"
		When clang runs with output "out-Os.o" and with extra clang flag "Os"

	Scenario: Check if we generate output
		Then every object should exist
