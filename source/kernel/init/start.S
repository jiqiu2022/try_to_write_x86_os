 	.text
 	.global _start
	.extern kernel_init
_start:
	call kernel_init
	jmp .