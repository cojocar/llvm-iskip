Feature: Check that the default flags are generating code duplication

	Background:
		Given a temporary directory output
		Given the bcfile of "empty-func.ll" as input

		When llc runs with output "empty.S" and with default flags
		When llc runs with output "passes0.S" and with passes
			| push-pop-replace |
		When llc runs with output "passes1.S" and with passes
			| push-pop-replace |
			| it-replace |
		When llc runs with output "passes2.S" and with passes
			| it-replace |
		When llc runs with output "passes3.S" and with passes
			| bl-replace |
		When llc runs with output "passes4.S" and with passes
			| load-store-update-replace |

		#When llc runs with output "adfasd.s" and with extra flag "flag"
		#When llc runs with output "adfasd.s" and with extra flags
		#	| flag1 |
		#	| flag2 |

	Scenario: Check if we have asm outputs and that they are all the same
		Then every asm should exist
		Then every asm output should be the same
