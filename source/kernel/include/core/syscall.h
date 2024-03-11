//
// Created by jiqiu2021 on 2024-03-06.
//
#include <sys/stat.h>
#ifndef OS_SYSCALL_H
#define OS_SYSCALL_H
#define SYSCALL_PARAM_COUNT     5       	// 系统调用最大支持的参数
#define SYS_msleep              0
#define SYS_getpid              1
#define SYS_printmsg            100
#define SYS_fork				2
#define SYS_execve				3
#define SYS_yield         4


#define SYS_open                50
#define SYS_read                51
#define SYS_write               52
#define SYS_close               53
#define SYS_lseek				54
#define SYS_isatty              55
#define SYS_sbrk                56
#define SYS_fstat               57
void exception_handler_syscall (void);		// syscall处理
typedef struct _syscall_frame_t {
    int eflags;
    int gs, fs, es, ds;
    int edi, esi, ebp, dummy, ebx, edx, ecx, eax;
    int eip, cs;
    int func_id, arg0, arg1, arg2, arg3;
    int esp, ss;
}syscall_frame_t;

#endif //OS_SYSCALL_H
