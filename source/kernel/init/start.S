  #include "os_cfg.h"
 
 
 	.text
 	.global _start
	.extern kernel_init
_start:
	call kernel_init
	jmp $KERNEL_SELECTOR_CS, $gdt_reload
	jmp .

gdt_reload:
	mov $KERNEL_SELECTOR_DS, %ax		// 16为数据段选择子
	mov %ax, %ds
    mov %ax, %ss
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

	// 栈设置
	mov $(stack + KERNEL_STACK_SIZE), %esp

	// 栈和段等沿用之前的设置
	jmp init_main

	.bss
.comm stack, KERNEL_STACK_SIZE    // comm 声明未初始化的通用内存区域，以字节计
