//
// Created by jiqiu2021 on 2024-03-09.
//

#ifndef OS_FS_H
#define OS_FS_H
int sys_open(const char *name, int flags, ...);
int sys_read(int file, char *ptr, int len);
int sys_write(int file, char *ptr, int len);
int sys_lseek(int file, int ptr, int dir);
int sys_close(int file);

#endif //OS_FS_H
