//
// Created by jiqiu2021 on 2024-03-06.
//

#ifndef OS_SYSCALL_H
#define OS_SYSCALL_H
#define SYSCALL_PARAM_COUNT     5       	// ϵͳ�������֧�ֵĲ���
#define SYS_msleep              0
#define SYS_getpid              1
#define SYS_printmsg            100
#define SYS_fork				2
#define SYS_execve				3


void exception_handler_syscall (void);		// syscall����
typedef struct _syscall_frame_t {
    int eflags;
    int gs, fs, es, ds;
    int edi, esi, ebp, dummy, ebx, edx, ecx, eax;
    int eip, cs;
    int func_id, arg0, arg1, arg2, arg3;
    int esp, ss;
}syscall_frame_t;

#endif //OS_SYSCALL_H
