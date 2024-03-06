//
// Created by jiqiu2021 on 2024-03-06.
//

#ifndef OS_LIB_SYSCALL_H
#define OS_LIB_SYSCALL_H
#include "os_cfg.h"
#include "core/syscall.h"

typedef struct _syscall_args{
    int id;
    int arg0;
    int arg1;
    int arg2;
    int arg3;
}syscall_args_t;

static inline int sys_call (syscall_args_t * args) {
    int ret;

    // 采用调用门, 这里只支持5个参数
    // 用调用门的好处是会自动将参数复制到内核栈中，这样内核代码很好取参数
    // 而如果采用寄存器传递，取参比较困难，需要先压栈再取
    __asm__ __volatile__(
            "int $0x80\n\n"
            :"=a"(ret)
            :"S"(args->arg3), "d"(args->arg2), "c"(args->arg1),
    "b"(args->arg0), "a"(args->id));
    return ret;
}
static inline int msleep (int ms) {
    if (ms <= 0) {
        return 0;
    }

    syscall_args_t args;
    args.id = SYS_msleep;
    args.arg0 = ms;
    return sys_call(&args);
}
static inline int get_pid () {

    syscall_args_t args;
    args.id = SYS_getpid;
    return sys_call(&args);
}

static inline int print_msg(char * fmt, int arg) {
    syscall_args_t args;
    args.id = SYS_printmsg;
    args.arg0 = (int)fmt;
    args.arg1 = arg;
    return sys_call(&args);
}
#endif //OS_LIB_SYSCALL_H
