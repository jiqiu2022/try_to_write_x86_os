//
// Created by jiqiu2021 on 2024-03-09.
//
#include "core/task.h"
#include "comm/cpu_instr.h"
#include "tools/klib.h"
#include "fs/fs.h"
#include "comm/boot_info.h"
#define TEMP_FILE_ID        100
#define TEMP_ADDR       (8*1024*1024)
static uint8_t * temp_pos;


static void read_disk(int sector, int sector_count, uint8_t * buf) {
    outb(0x1F6, (uint8_t) (0xE0));

    outb(0x1F2, (uint8_t) (sector_count >> 8));
    outb(0x1F3, (uint8_t) (sector >> 24));		// LBA������24~31λ
    outb(0x1F4, (uint8_t) (0));					// LBA������32~39λ
    outb(0x1F5, (uint8_t) (0));					// LBA������40~47λ

    outb(0x1F2, (uint8_t) (sector_count));
    outb(0x1F3, (uint8_t) (sector));			// LBA������0~7λ
    outb(0x1F4, (uint8_t) (sector >> 8));		// LBA������8~15λ
    outb(0x1F5, (uint8_t) (sector >> 16));		// LBA������16~23λ

    outb(0x1F7, (uint8_t) 0x24);

    // ��ȡ����
    uint16_t *data_buf = (uint16_t*) buf;
    while (sector_count-- > 0) {
        // ÿ��������֮ǰ��Ҫ��飬�ȴ����ݾ���
        while ((inb(0x1F7) & 0x88) != 0x8) {}

        // ��ȡ��������д�뵽������
        for (int i = 0; i < SECTOR_SIZE / 2; i++) {
            *data_buf++ = inw(0x1F0);
        }
    }
}

int sys_open(const char *name, int flags, ...){
    if (name[0]=='/'){
        read_disk(5000,80,(uint8_t* )TEMP_ADDR);
        temp_pos =(uint8_t*)TEMP_ADDR;
        return TEMP_FILE_ID;
    }
    return -1;
}
int sys_read(int file, char *ptr, int len){
    if (file==TEMP_FILE_ID){
        kernel_memcpy(ptr,temp_pos,len);
        temp_pos+=len;
        return len;
    }
    return -1;
}
int sys_write(int file, char *ptr, int len){
    return -1;
}
int sys_lseek(int file, int ptr, int dir) {
    if (file == TEMP_FILE_ID) {

        // ����д����
        temp_pos = (uint8_t *)(ptr + TEMP_ADDR);
        return 0;
    }
    return -1;
}
int sys_close(int file){
    return -1;
}
