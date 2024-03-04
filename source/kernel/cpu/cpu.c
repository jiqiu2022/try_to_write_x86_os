#include "cpu/cpu.h"
#include "os_cfg.h"
#include "comm/cpu_instr.h"
#include "ipc/mutex.h"
static segment_desc_t gdt_table[GDT_TABLE_SIZE];
static mutex_t mutex;

int gdt_alloc_desc(void){
    mutex_lock(&mutex);
    for (int i = 1; i < GDT_TABLE_SIZE; i++)
    {
        segment_desc_t *desc =gdt_table+i;
        if (desc->attr==0)
        {
            return i *sizeof(segment_desc_t);
        }
        
    }
    mutex_unlock(&mutex);
    return -1;
    
}
void switch_to_tss (uint32_t tss_selector) {
    far_jump(tss_selector, 0);
}
void segment_desc_set(int selector,uint32_t base,uint32_t limit,uint16_t attr){
    segment_desc_t * desc =gdt_table+selector/sizeof(segment_desc_t);
    

    if (limit >0xfffff)
    {
        attr |=0x8000;
        limit/=0x1000;
        
        
    }
    
    desc->limit15_0 =limit&0xffff;
    desc->base15_0 =base&0xffff;
    desc->base23_16=(base>>16)&0xff;
    desc->attr =attr | (((limit >> 16) & 0xf) << 8);
    desc->base31_24 = (base >> 24) & 0xff;
}
void gate_desc_set(gate_desc_t * desc, uint16_t selector, uint32_t offset, uint16_t attr) {
	desc->offset15_0 = offset & 0xffff;
	desc->selector = selector;
	desc->attr = attr;
	desc->offset31_16 = (offset >> 16) & 0xffff;
}
void gdt_free_sel (int sel) {
    mutex_lock(&mutex);
    gdt_table[sel / sizeof(segment_desc_t)].attr = 0;
    mutex_unlock(&mutex);
}
void init_gdt(void){
    for (int i = 0; i < GDT_TABLE_SIZE; i++)
    {
        segment_desc_set(i*sizeof(segment_desc_t),0,0,0);
        
    }
    segment_desc_set(KERNEL_SELECTOR_DS,0x00000000, 0xFFFFFFFF,SEG_P_PRESENT | SEG_DPL0 | SEG_S_NORMAL | SEG_TYPE_DATA
                     | SEG_TYPE_RW | SEG_D);
    segment_desc_set(KERNEL_SELECTOR_CS, 0x00000000, 0xFFFFFFFF,
                     SEG_P_PRESENT | SEG_DPL0 | SEG_S_NORMAL | SEG_TYPE_CODE
                     | SEG_TYPE_RW | SEG_D);
     // 加载gdt
    lgdt((uint32_t)gdt_table, sizeof(gdt_table));
} 

void cpu_init(){
    mutex_init(&mutex);
    init_gdt();
}

