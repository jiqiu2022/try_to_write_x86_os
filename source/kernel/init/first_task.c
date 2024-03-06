//
// Created by jiqiu2021 on 2024-03-05.
//


#include "core/task.h"
#include "tools/log.h"
#include "applib/lib_syscall.h"


int first_task_main(void){
    int pid = get_pid();

    for (;;) {
        print_msg("task id = %d", pid);
        msleep(1000);
    }

    return 0;
}