//
// Created by jiqiu2021 on 2024-03-05.
//


#include "core/task.h"
#include "tools/log.h"


int first_task_main(void){
    for (;;){
        log_printf("first task.");
        sys_msleep(1000);
    }
    return 0;
}