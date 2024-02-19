#include "cpu/irq.h"
#include "cpu/cpu.h"
#include "comm/cpu_instr.h"
#include "os_cfg.h"
#define IDT_TABLE_NR			128				// IDT表项数量
static gate_desc_t idt_table[IDT_TABLE_NR];	// 中断描述表
void irq_init(void) {	
	for (uint32_t i = 0; i < IDT_TABLE_NR; i++) {
        gate_desc_set(idt_table + i, 0, 0, 0);
	}
	lidt((uint32_t)idt_table, sizeof(idt_table));
}
