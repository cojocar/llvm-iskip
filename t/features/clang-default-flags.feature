Feature: Check that the default flags (clang) are generating code duplication

	Background:
		Given a temporary directory output
		Given the C file "empty-func.c" as input

		When clang runs with output "empty.o" and with default flags
		When clang runs with output "passes0.o" and with passes
			| push-pop-replace |
		When clang runs with output "passes1.o" and with passes
			| push-pop-replace |
			| it-replace |
		When clang runs with output "passes2.o" and with passes
			| it-replace |
		When clang runs with output "passes3.o" and with passes
			| bl-replace |
		When clang runs with output "passes4.o" and with passes
			| load-store-update-replace |

		#	When clang runs with output "adfasd.o" and with extra clang flag "flag"
		#	When clang runs with output "adfasd.o" and with extra clang flags
		#	| flag1 |
		#	| flag2 |

	Scenario: Check if we generate output
		Then every object should exist
