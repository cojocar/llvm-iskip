Feature: Check that we generate two adds

	Background:
		Given a temporary directory output
		Given the bcfile of "simple-add.ll" as input

		When llc runs with output "out-O0.S" and with extra flag "O0"
		When llc runs with output "out-O1.S" and with extra flag "O1"
		When llc runs with output "out-O2.S" and with extra flag "O2"
		When llc runs with output "out-O3.S" and with extra flag "O3"

	Scenario: Check if we generate output
		Then every asm should exist

	Scenario: Check if the add instruction is duplicated on all outputs
		Then the following command on every asm output should be true "test `grep add < %out% | wc -l` -ge 2"

	Scenario: Check if the add instruction is duplicated on all outputs (accurate)
		Then the following command on "out-O0.S" asm should be true "test `grep add.w < %out% | wc -l` -eq 2"
		Then the following command on "out-O1.S" asm should be true "test `grep adds < %out% | wc -l` -eq 2"
		Then the following command on "out-O2.S" asm should be true "test `grep adds < %out% | wc -l` -eq 2"
		Then the following command on "out-O3.S" asm should be true "test `grep adds < %out% | wc -l` -eq 2"
