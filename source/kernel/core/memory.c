//
// Created by jiqiu2021 on 2024-03-04.
//

#include "core/memory.h"
#include "tools/log.h"
#include "tools/klib.h"
#include "cpu/mmu.h"
static addr_alloc_t paddr_alloc;        // 物理地址分配结构
static pde_t kernel_page_dir[PDE_CNT] __attribute__((aligned(MEM_PAGE_SIZE))); // 内核页目录表



pte_t *find_pte();

/**
 * @brief 获取空闲页表
 * @param alloc
 * @param count
 * @return
 */
static  uint32_t addr_alloc_page(addr_alloc_t* alloc,int count){
    uint32_t addr=0;
    mutex_lock(&alloc->mutex);

    int page_index = bitmap_alloc_nbits(&alloc->bitmap,0,count);
    if(page_index>=0){
        addr =alloc->start+page_index*alloc->page_size;

    }
    mutex_unlock(&alloc->mutex);
    return addr;
}
static void addr_alloc_init(addr_alloc_t* alloc,uint8_t* bits,uint32_t start,uint32_t size,uint32_t page_size){
    mutex_init(&alloc->mutex);
    alloc->start=start;
    alloc->size=size;
    alloc->page_size=page_size;
    bitmap_init(&alloc->bitmap,bits,alloc->size/page_size,0);

}

/**
 * @brief 释放多页内存
 * @param alloc
 * @param addr
 * @param page_count
 */
static void addr_free_page(addr_alloc_t *alloc,uint32_t addr,int page_count){
    mutex_lock(&alloc->mutex);
    uint32_t  pg_idx=(addr-alloc->start);
    bitmap_set_bit(&alloc->bitmap,pg_idx,page_count,0);
    mutex_unlock(&alloc->mutex);
}
static void show_mem_info (boot_info_t * boot_info) {
    log_printf("mem region:");
    for (int i = 0; i < boot_info->ram_region_count; i++) {
        log_printf("[%d]: 0x%x - 0x%x", i,
                   boot_info->ram_region_cfg[i].start,
                   boot_info->ram_region_cfg[i].size);
    }
    log_printf("\n");
}

/**
 * @brief 获取所有可用内存
 * @param boot_info
 * @return
 */
static uint32_t total_mem_size(boot_info_t * boot_info) {
    int mem_size = 0;

    // 简单起见，暂不考虑中间有空洞的情况
    for (int i = 0; i < boot_info->ram_region_count; i++) {
        mem_size += boot_info->ram_region_cfg[i].size;
    }
    return mem_size;
}
int memory_create_map (pde_t * page_dir, uint32_t vaddr, uint32_t paddr, int count, uint32_t perm) {
    for (int i = 0; i < count; i++) {
        //log_printf("create map: v-0x%x p-0x%x, perm: 0x%x", vaddr, paddr, perm);

        pte_t * pte = find_pte(page_dir, vaddr, 1);
        if (pte == (pte_t *)0) {
            //log_printf("create pte failed. pte == 0");
            return -1;
        }

        // 创建映射的时候，这条pte应当是不存在的。
        // 如果存在，说明可能有问题
        log_printf("\tpte addr: 0x%x", (uint32_t)pte);
        ASSERT(pte->present == 0);

        pte->v = paddr | perm | PTE_P;

        vaddr += MEM_PAGE_SIZE;
        paddr += MEM_PAGE_SIZE;
    }

    return 0;
}

/**
 * @brief 创建进程的初始页表
 * 主要的工作创建页目录表，然后从内核页表中复制一部分
 */
uint32_t memory_create_uvm (void) {
    pde_t * page_dir = (pde_t *)addr_alloc_page(&paddr_alloc, 1);
    if (page_dir == 0) {
        return 0;
    }
    kernel_memset((void *)page_dir, 0, MEM_PAGE_SIZE);

    // 复制整个内核空间的页目录项，以便与其它进程共享内核空间
    // 用户空间的内存映射暂不处理，等加载程序时创建
    uint32_t user_pde_start = pde_index(MEMORY_TASK_BASE);
    for (int i = 0; i < user_pde_start; i++) {
        page_dir[i].v = kernel_page_dir[i].v;
    }

    return (uint32_t)page_dir;
}
void create_kernel_table(void){
    // 获取ld脚本提供的外部符号，使用数组方式直接拿到数据
    extern  uint8_t s_text[],e_text[],s_data[],e_data[];
    extern  uint8_t  kernel_base[];
    static memory_map_t kernel_map[] = {
            {kernel_base,   s_text,         0,              PTE_W},         // 内核栈区
            {s_text,        e_text,         s_text,         0},         // 内核代码区
            {s_data,        (void *)(MEM_EBDA_START - 1),   s_data,        PTE_W},      // 内核数据区

            // 扩展存储空间一一映射，方便直接操作
            {(void *)MEM_EXT_START, (void *)MEM_EXT_END,     (void *)MEM_EXT_START, PTE_W},
    };

    for (int i = 0; i < sizeof (kernel_map)/sizeof(memory_map_t) ; ++i) {
        memory_map_t * map=kernel_map+i;
        int vstart = down2((uint32_t)map->vstart, MEM_PAGE_SIZE);
        int vend = up2((uint32_t)map->vend, MEM_PAGE_SIZE);
        int page_count = (vend - vstart) / MEM_PAGE_SIZE;

        memory_create_map(kernel_page_dir, vstart, (uint32_t)map->pstart, page_count, map->perm);
        //创建内核映射表


    }
}


pte_t *find_pte(pde_t * page_dir,uint32_t vaddr,int alloc) {
    pte_t * page_table;
    pde_t *pde =page_dir+ pde_index(vaddr);
    if (pde->present){
        page_table=(pte_t *) pde_paddr(pde);
    } else{
        //分配流程
        if (alloc==0){
            return (pte_t *)0;
        }
        uint32_t  pg_paddr= addr_alloc_page(&paddr_alloc,1);
        if(pg_paddr==0){
            return (pte_t *)0;
        }
        pde->v=pg_paddr|PTE_P;

        page_table =(pte_t *)(pg_paddr);
        kernel_memset(page_table,0,MEM_PAGE_SIZE);

    }
    return page_table + pte_index(vaddr);
}

void memory_init(boot_info_t *bootInfo){
    extern uint8_t * mem_free_start;
    log_printf("mem init.");
    show_mem_info(bootInfo);

    addr_alloc_t addr_alloc;
    uint8_t bits[8];
    uint8_t * mem_free = (uint8_t *)&mem_free_start;
    uint32_t mem_up1MB_free = total_mem_size(bootInfo) - MEM_EXT_START;
    mem_up1MB_free=down2(mem_up1MB_free,MEM_PAGE_SIZE);
    log_printf("Free memory: 0x%x, size: 0x%x", MEM_EXT_START, mem_up1MB_free);

    addr_alloc_init (&paddr_alloc,mem_free,MEM_EXT_START,mem_up1MB_free,MEM_PAGE_SIZE);
    mem_free += bitmap_byte_count(paddr_alloc.size / MEM_PAGE_SIZE);

    // 到这里，mem_free应该比EBDA地址要小
    ASSERT(mem_free < (uint8_t *)MEM_EBDA_START);

    // 创建内核页表并切换过去
    create_kernel_table();

    // 先切换到当前页表
    mmu_set_page_dir((uint32_t)kernel_page_dir);
}
