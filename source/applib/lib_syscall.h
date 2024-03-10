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

static inline  int sys_call(syscall_args_t *args){
    const unsigned long sys_gate_addr[]={
            0, SELECTOR_SYSCALL | 0
    };
    int ret;
    __asm__ __volatile__(
            "push %[arg3]\n\t"
            "push %[arg2]\n\t"
            "push %[arg1]\n\t"
            "push %[arg0]\n\t"
            "push %[id]\n\t"
            "lcalll *(%[gate])\n\n"
            :"=a"(ret)
            :[arg3]"r"(args->arg3), [arg2]"r"(args->arg2), [arg1]"r"(args->arg1),
    [arg0]"r"(args->arg0), [id]"r"(args->id),
    [gate]"r"(sys_gate_addr));
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


static inline int fork() {
    syscall_args_t args;
    args.id = SYS_fork;
    return sys_call(&args);
}

static inline  int execve(const char *name,char * const *argv, char * const *env){
    syscall_args_t args;
    args.id=SYS_execve;
    args.arg0=(int)name;
    args.arg1=(int)argv;
    args.arg2=(int)env;
    return sys_call(&args);

}
#endif //OS_LIB_SYSCALL_H
