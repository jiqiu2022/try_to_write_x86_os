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



    .text
	.extern do_handler_unknown
	.global exception_handler_unknown
.macro exception_handler name num with_error_code
	    .extern do_handler_\name
		.global exception_handler_\name
	exception_handler_\name:
		// 如果没有错误码，压入一个缺省值
		// 这样堆栈就和有错误码的情形一样了
		.if \with_error_code == 0
			push $0
		.endif

		// 压入异常号
		push $\num

		// 保存所有寄存器
		pushal
		push %ds
		push %es
		push %fs
		push %gs

		// 调用中断处理函数
		push %esp
		call do_handler_\name
		add $(1*4), %esp		// 丢掉esp

		// 恢复保存的寄存器
		pop %gs
		pop %fs
		pop %es
		pop %ds
		popal

		// 跳过压入的异常号和错误码
		add $(2*4), %esp
		iret
.endm

exception_handler unknown, -1, 0
exception_handler divider, 0, 0