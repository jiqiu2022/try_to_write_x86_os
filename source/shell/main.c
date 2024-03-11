#include "lib_syscall.h"
#include <stdio.h>
int main (int argc, char **argv) {
    for (int i = 0; i < argc; i++) {
        print_msg("arg: %s", (int)argv[i]);
    }
    puts("hello from x86 os");
    printf("os version: %s\n", OS_VERSION);
    puts("author: jiqiu2021");
    puts("create data: 2022-5-31");
    // 创建一个自己的副本
    int pid =fork();
    if (pid==0){
        print_msg("fork sucessful",0);
    }


    for (;;) {
        print_msg("pid=%d", getpid());
        msleep(1000);
    }
}