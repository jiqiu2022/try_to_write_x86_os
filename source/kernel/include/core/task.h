#ifndef TASK_H
#define TASK_H
#include "cpu/cpu.h"
#include "comm/types.h"


typedef struct _task_t{
    tss_t tss;
    uint16_t tss_sel;		// tss选择子
}task_t;

void task_switch_from_to (task_t * from, task_t * to);

int task_init (task_t *task, uint32_t entry, uint32_t esp);
#endif