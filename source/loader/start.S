  	.code16
 	.text
 	.extern loader_entry
	.global _start
_start:

	jmp loader_entry