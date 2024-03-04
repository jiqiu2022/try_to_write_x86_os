//
// Created by jiqiu2021 on 2024-03-04.
//

#include "core/memory.h"
#include "tools/log.h"

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
        addr =alloc->start+page_index*alloc->start;

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
void memory_init(boot_info_t *bootInfo){
    addr_alloc_t addr_alloc;
    uint8_t bits[8];

    addr_alloc_init (&addr_alloc,bits,0x1000,64*4096,4096);
    for (int i = 0; i < 32; i++) {
        uint32_t addr = addr_alloc_page(&addr_alloc, 2);
        log_printf("alloc addr: 0x%x", addr);
    }

    uint32_t addr = 0;
    for (int i = 0; i < 32; i++) {
        addr_free_page(&addr_alloc, addr, 2);
        addr += 8192;
    }
}
