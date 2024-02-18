  	.code16
 	.text
 	.extern loader_entry
	.global _start
_start:

	jmp loader_entry


	.code32
	.text
	.global protect_mode_entry
	.extern load_kernel
protect_mode_entry:
	mov $16,%ax
	mov %ax,%ds
	mov %ax, %ss
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

	push %ebp
	mov %esp,%ebp
	mov 0x8(%ebp),%eax
	push %eax

	jmp $8, $load_kernel