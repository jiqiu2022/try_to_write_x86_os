#include "lib_syscall.h"

int main (int argc, char **argv) {
    for (int i = 0; i < argc; i++) {
        print_msg("arg: %s", (int)argv[i]);
    }

    // 创建一个自己的副本
    int pid =fork();
    if (pid==0){
        print_msg("fork sucessful",0);
    }


    for (;;) {
        print_msg("pid=%d", get_pid());
        msleep(1000);
    }
}