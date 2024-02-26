#include "comm/boot_info.h"
#include "cpu/cpu.h"
#include "cpu/irq.h"
/**
 * 内核入口
 */
static boot_info_t * init_boot_info; 
void kernel_init (boot_info_t * boot_info) {
    cpu_init();
    irq_init();
    init_boot_info =boot_info;
}
void init_main(void) {
     int a = 3 / 0;
    for (;;) {}
}
