//
// Created by jiqiu2021 on 2024-03-04.
//

#ifndef OS_MEMORY_H
#define OS_MEMORY_H
#include "tools/bitmap.h"
#include "comm/boot_info.h"
#include "ipc/mutex.h"


typedef struct _addr_alloc_t{
    mutex_t mutex;
    bitmap_t bitmap;

    uint32_t page_size;
    uint32_t start;
    uint32_t size;
}addr_alloc_t;
typedef struct _memory_map_t {
    void * vstart;     // 虚拟地址
    void * vend;
    void * pstart;       // 物理地址
    uint32_t perm;      // 访问权限
}memory_map_t;
void memory_init(boot_info_t *bootInfo);
uint32_t memory_create_uvm (void);
#define MEM_EBDA_START              0x00080000
#define MEM_EXT_START               (1024*1024)
#define MEM_PAGE_SIZE               4096        // 和页表大小一致
#define MEMORY_TASK_BASE        (0x80000000)        // 进程起始地址空间
#define MEM_EXT_END                 (128*1024*1024 - 1)
#define MEM_TASK_STACK_TOP          (0xE0000000)
#define MEM_TASK_STACK_SIZE         (MEM_PAGE_SIZE * 500)   // 初始500KB栈
#define MEM_TASK_ARG_SIZE           (MEM_PAGE_SIZE * 4)     // 参数和环境变量占用的大小

int memory_alloc_page_for(uint32_t addr,uint32_t size,int perm);
uint32_t memory_alloc_page (void);
void memory_free_page (uint32_t addr);
uint32_t memory_copy_uvm (uint32_t page_dir);
void memory_destroy_uvm(uint32_t page_dir);
uint32_t memory_alloc_for_page_dir(uint32_t page_dir,uint32_t vaddr,uint32_t size,int perm);
uint32_t memory_get_paddr (uint32_t page_dir, uint32_t vaddr) ;
int memory_copy_uvm_data(uint32_t to, uint32_t page_dir, uint32_t from, uint32_t size);
#endif //OS_MEMORY_H
