Feature: Check that llc generates some code when running with opts

	Background:
		Given a temporary directory output
		Given the bcfile of "empty-func.ll" as input

		When llc runs with output "out-O0.o" and with extra flag "O0"
		When llc runs with output "out-O1.o" and with extra flag "O1"
		When llc runs with output "out-O2.o" and with extra flag "O2"
		When llc runs with output "out-O3.o" and with extra flag "O3"

	Scenario: Check if we generate output
		Then every asm should exist
