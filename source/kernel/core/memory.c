//
// Created by jiqiu2021 on 2024-03-04.
//

#include "core/memory.h"
#include "tools/log.h"
#include "tools/klib.h"
#include "cpu/mmu.h"
int memory_create_map (pde_t * page_dir, uint32_t vaddr, uint32_t paddr, int count, uint32_t perm);
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
uint32_t memory_get_paddr (uint32_t page_dir, uint32_t vaddr) {
    pte_t * pte = find_pte((pde_t *)page_dir, vaddr, 0);
    if (pte == (pte_t *)0) {
        return 0;
    }

    return pte_paddr(pte) + (vaddr & (MEM_PAGE_SIZE - 1));
}
uint32_t memory_alloc_for_page_dir(uint32_t page_dir,uint32_t vaddr,uint32_t size,int perm){
    uint32_t curr_vaddr =vaddr;
    int page_count= up2(size,MEM_PAGE_SIZE)/MEM_PAGE_SIZE;
    vaddr= down2(vaddr,MEM_PAGE_SIZE);
    for (int i = 0; i < page_count; ++i) {
        uint32_t paddr= addr_alloc_page(&paddr_alloc,1);
        ASSERT(paddr!=0);



        int err=memory_create_map((pde_t*)page_dir,curr_vaddr,paddr,1,perm);
        if (err<0){
            log_printf("create memory map failed. err = %d", err);
            addr_free_page(&paddr_alloc, vaddr, i + 1);
            return -1;
        }
        curr_vaddr += MEM_PAGE_SIZE;
    }
    return 0;
}


int memory_alloc_page_for(uint32_t addr,uint32_t size,int perm){
    return memory_alloc_for_page_dir(task_current()->tss.cr3, addr, size, perm);

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
        //log_printf("\tpte addr: 0x%x", (uint32_t)pte);
        ASSERT(pte->present == 0);

        pte->v = paddr | perm | PTE_P;

        vaddr += MEM_PAGE_SIZE;
        paddr += MEM_PAGE_SIZE;
    }

    return 0;
}
int memory_copy_uvm_data(uint32_t to, uint32_t page_dir, uint32_t from, uint32_t size) {
    char *buf, *pa0;

    while(size > 0){
        // 获取目标的物理地址, 也即其另一个虚拟地址
        uint32_t to_paddr = memory_get_paddr(page_dir, to);
        if (to_paddr == 0) {
            return -1;
        }

        // 计算当前可拷贝的大小
        uint32_t offset_in_page = to_paddr & (MEM_PAGE_SIZE - 1);
        uint32_t curr_size = MEM_PAGE_SIZE - offset_in_page;
        if (curr_size > size) {
            curr_size = size;       // 如果比较大，超过页边界，则只拷贝此页内的
        }

        kernel_memcpy((void *)to_paddr, (void *)from, curr_size);

        size -= curr_size;
        to += curr_size;
        from += curr_size;
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
        pde->v = pg_paddr | PTE_P | PTE_W | PDE_U;
        page_table =(pte_t *)(pg_paddr);
        kernel_memset(page_table,0,MEM_PAGE_SIZE);

    }
    return page_table + pte_index(vaddr);
}
uint32_t memory_alloc_page (void) {
    // 内核空间虚拟地址与物理地址相同
    return addr_alloc_page(&paddr_alloc, 1);
}
static pde_t * current_page_dir (void) {
    return (pde_t *)task_current()->tss.cr3;
}
void memory_free_page (uint32_t addr) {
    if (addr < MEMORY_TASK_BASE) {
        // 内核空间，直接释放
        addr_free_page(&paddr_alloc, addr, 1);
    } else {
        // 进程空间，还要释放页表
        pte_t * pte = find_pte(current_page_dir(), addr, 0);
        ASSERT((pte == (pte_t *)0) && pte->present);

        // 释放内存页
        addr_free_page(&paddr_alloc, pte_paddr(pte), 1);

        // 释放页表
        pte->v = 0;
    }
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
void memory_destroy_uvm(uint32_t page_dir){
    uint32_t user_pde_start = pde_index(MEMORY_TASK_BASE);
    pde_t* pde=(pde_t*)page_dir+user_pde_start;
    for (int i = user_pde_start; i < PDE_CNT; ++i,pde++) {
        if (!pde->present){
            continue;
        } else{
            pte_t * pte = (pte_t *)pde_paddr(pde);
            for (int j = 0; j < PTE_CNT; j++, pte++) {
                if (!pte->present) {
                    continue;
                }

                addr_free_page(&paddr_alloc, pte_paddr(pte), 1);
            }
            addr_free_page(&paddr_alloc, pte_paddr(pte), 1);
        }
        addr_free_page(&paddr_alloc, (uint32_t)pde_paddr(pde), 1);

    }
}


uint32_t memory_copy_uvm (uint32_t page_dir){
    uint32_t  to_page_dir =memory_create_uvm();
    if(to_page_dir==0){
        log_printf("create page dir failed");
        goto copy_uvm_failed;
    }

    uint32_t user_pde_start = pde_index(MEMORY_TASK_BASE);
    pde_t * pde = (pde_t *)page_dir + user_pde_start;

    // 遍历用户空间页目录项
    for (int i = user_pde_start; i < PDE_CNT; i++, pde++) {
        if (!pde->present) {
            continue;
        }

        // 遍历页表
        pte_t * pte = (pte_t *)pde_paddr(pde);
        for (int j = 0; j < PTE_CNT; j++, pte++) {
            if (!pte->present) {
                continue;
            }

            // 分配物理内存
            uint32_t page = addr_alloc_page(&paddr_alloc, 1);
            if (page == 0) {
                goto copy_uvm_failed;
            }

            // 建立映射关系
            uint32_t vaddr = (i << 22) | (j << 12);
            int err = memory_create_map((pde_t *)to_page_dir, vaddr, page, 1, get_pte_perm(pte));
            if (err < 0) {
                goto copy_uvm_failed;
            }

            // 复制内容。
            kernel_memcpy((void *)page, (void *)vaddr, MEM_PAGE_SIZE);
        }
    }
    return to_page_dir;
copy_uvm_failed:
    if(to_page_dir){
        memory_destroy_uvm(to_page_dir);
    }
}
char * sys_sbrk(int incr) {
    task_t * task = task_current();
    char * pre_heap_end = (char * )task->heap_end;
    int pre_incr = incr;

    ASSERT(incr >= 0);

    // 如果地址为0，则返回有效的heap区域的顶端
    if (incr == 0) {
        log_printf("sbrk(0): end = 0x%x", pre_heap_end);
        return pre_heap_end;
    }

    uint32_t start = task->heap_end;
    uint32_t end = start + incr;

    // 起始偏移非0
    int start_offset = start % MEM_PAGE_SIZE;
    if (start_offset) {
        // 不超过1页，只调整
        if (start_offset + incr <= MEM_PAGE_SIZE) {
            task->heap_end = end;
            return pre_heap_end;
        } else {
            // 超过1页，先只调本页的
            uint32_t curr_size = MEM_PAGE_SIZE - start_offset;
            start += curr_size;
            incr -= curr_size;
        }
    }

    // 处理其余的，起始对齐的页边界的
    if (incr) {
        uint32_t curr_size = end - start;
        int err = memory_alloc_page_for(start, curr_size, PTE_P | PTE_U | PTE_W);
        if (err < 0) {
            log_printf("sbrk: alloc mem failed.");
            return (char *)-1;
        }
    }

    log_printf("sbrk(%d): end = 0x%x", pre_incr, end);
    task->heap_end = end;
    return (char * )pre_heap_end;
}