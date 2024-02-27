#include "comm/boot_info.h"
#include "cpu/cpu.h"
#include "cpu/irq.h"
#include "dev/time.h"
#include "tools/log.h"
#include "os_cfg.h"
#include "tools/klib.h"
/**
 * 内核入口
 */
static boot_info_t * init_boot_info; 
void kernel_init (boot_info_t * boot_info) {
    cpu_init();
    log_init();
    irq_init();
    time_init();
    init_boot_info =boot_info;
}
void init_main(void) {
    log_printf("Kernel is running....");
    log_printf("Version: %s, name: %s", OS_VERSION, "tiny x86 os");
    log_printf("%d %d %x %c", -123, 123456, 0x12345, 'a');
    //  int a = 3 / 0;
    int a = 3;
    ASSERT(a > 2);
    ASSERT(a < 2);
    for (;;) {}
}
