//
// Created by jiqiu2021 on 2024-03-06.
//

#ifndef OS_LIB_SYSCALL_H
#define OS_LIB_SYSCALL_H

#include "core/syscall.h"
#include "os_cfg.h"
#include <sys/stat.h>
typedef struct _syscall_args_t {
    int id;
    int arg0;
    int arg1;
    int arg2;
    int arg3;
}syscall_args_t;

int msleep (int ms);
int fork(void);
int getpid(void);
int yield (void);
int execve(const char *name, char * const *argv, char * const *env);
int print_msg(char * fmt, int arg);

int open(const char *name, int flags, ...);
int read(int file, char *ptr, int len);
int write(int file, char *ptr, int len);
int close(int file);
int lseek(int file, int ptr, int dir);
#endif //OS_LIB_SYSCALL_H
