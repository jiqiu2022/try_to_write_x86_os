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
void memory_init(boot_info_t *bootInfo);

#endif //OS_MEMORY_H
