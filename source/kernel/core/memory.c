//
// Created by jiqiu2021 on 2024-03-04.
//

#include "core/memory.h"
#include "tools/log.h"
#include "tools/klib.h"
#include "cpu/mmu.h"
int memory_create_map (pde_t * page_dir, uint32_t vaddr, uint32_t paddr, int count, uint32_t perm);
static addr_alloc_t paddr_alloc;        // �����ַ����ṹ
static pde_t kernel_page_dir[PDE_CNT] __attribute__((aligned(MEM_PAGE_SIZE))); // �ں�ҳĿ¼��



pte_t *find_pte();

/**
 * @brief ��ȡ����ҳ��
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
 * @brief �ͷŶ�ҳ�ڴ�
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
 * @brief ��ȡ���п����ڴ�
 * @param boot_info
 * @return
 */
static uint32_t total_mem_size(boot_info_t * boot_info) {
    int mem_size = 0;

    // ��������ݲ������м��пն������
    for (int i = 0; i < boot_info->ram_region_count; i++) {
        mem_size += boot_info->ram_region_cfg[i].size;
    }
    return mem_size;
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

        // ����ӳ���ʱ������pteӦ���ǲ����ڵġ�
        // ������ڣ�˵������������
        //log_printf("\tpte addr: 0x%x", (uint32_t)pte);
        ASSERT(pte->present == 0);

        pte->v = paddr | perm | PTE_P;

        vaddr += MEM_PAGE_SIZE;
        paddr += MEM_PAGE_SIZE;
    }

    return 0;
}

/**
 * @brief �������̵ĳ�ʼҳ��
 * ��Ҫ�Ĺ�������ҳĿ¼��Ȼ����ں�ҳ���и���һ����
 */
uint32_t memory_create_uvm (void) {
    pde_t * page_dir = (pde_t *)addr_alloc_page(&paddr_alloc, 1);
    if (page_dir == 0) {
        return 0;
    }
    kernel_memset((void *)page_dir, 0, MEM_PAGE_SIZE);

    // ���������ں˿ռ��ҳĿ¼��Ա����������̹����ں˿ռ�
    // �û��ռ���ڴ�ӳ���ݲ������ȼ��س���ʱ����
    uint32_t user_pde_start = pde_index(MEMORY_TASK_BASE);
    for (int i = 0; i < user_pde_start; i++) {
        page_dir[i].v = kernel_page_dir[i].v;
    }

    return (uint32_t)page_dir;
}
void create_kernel_table(void){
    // ��ȡld�ű��ṩ���ⲿ���ţ�ʹ�����鷽ʽֱ���õ�����
    extern  uint8_t s_text[],e_text[],s_data[],e_data[];
    extern  uint8_t  kernel_base[];
    static memory_map_t kernel_map[] = {
            {kernel_base,   s_text,         0,              PTE_W},         // �ں�ջ��
            {s_text,        e_text,         s_text,         0},         // �ں˴�����
            {s_data,        (void *)(MEM_EBDA_START - 1),   s_data,        PTE_W},      // �ں�������

            // ��չ�洢�ռ�һһӳ�䣬����ֱ�Ӳ���
            {(void *)MEM_EXT_START, (void *)MEM_EXT_END,     (void *)MEM_EXT_START, PTE_W},
    };

    for (int i = 0; i < sizeof (kernel_map)/sizeof(memory_map_t) ; ++i) {
        memory_map_t * map=kernel_map+i;
        int vstart = down2((uint32_t)map->vstart, MEM_PAGE_SIZE);
        int vend = up2((uint32_t)map->vend, MEM_PAGE_SIZE);
        int page_count = (vend - vstart) / MEM_PAGE_SIZE;

        memory_create_map(kernel_page_dir, vstart, (uint32_t)map->pstart, page_count, map->perm);
        //�����ں�ӳ���


    }
}


pte_t *find_pte(pde_t * page_dir,uint32_t vaddr,int alloc) {
    pte_t * page_table;
    pde_t *pde =page_dir+ pde_index(vaddr);
    if (pde->present){
        page_table=(pte_t *) pde_paddr(pde);
    } else{
        //��������
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
    // �ں˿ռ������ַ�������ַ��ͬ
    return addr_alloc_page(&paddr_alloc, 1);
}
static pde_t * current_page_dir (void) {
    return (pde_t *)task_current()->tss.cr3;
}
void memory_free_page (uint32_t addr) {
    if (addr < MEMORY_TASK_BASE) {
        // �ں˿ռ䣬ֱ���ͷ�
        addr_free_page(&paddr_alloc, addr, 1);
    } else {
        // ���̿ռ䣬��Ҫ�ͷ�ҳ��
        pte_t * pte = find_pte(current_page_dir(), addr, 0);
        ASSERT((pte == (pte_t *)0) && pte->present);

        // �ͷ��ڴ�ҳ
        addr_free_page(&paddr_alloc, pte_paddr(pte), 1);

        // �ͷ�ҳ��
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

    // �����mem_freeӦ�ñ�EBDA��ַҪС
    ASSERT(mem_free < (uint8_t *)MEM_EBDA_START);

    // �����ں�ҳ���л���ȥ
    create_kernel_table();

    // ���л�����ǰҳ��
    mmu_set_page_dir((uint32_t)kernel_page_dir);
}
