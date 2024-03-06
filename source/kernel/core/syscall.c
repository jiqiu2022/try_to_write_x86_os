
#include "core/syscall.h"
#include "tools/klib.h"
#include "core/task.h"
#include "tools/log.h"
#include "cpu/irq.h"
// ϵͳ���ô���������
typedef int (*syscall_handler_t)(uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);
int sys_print_msg (char * fmt, int arg) ;
// ϵͳ���ñ�
static const syscall_handler_t sys_table[] = {
        [SYS_msleep] = (syscall_handler_t)sys_msleep,
        [SYS_getpid] =(syscall_handler_t)sys_getpid,
        [SYS_printmsg] = (syscall_handler_t)sys_print_msg,


};
int sys_print_msg (char * fmt, int arg) {
    log_printf(fmt, arg);
}
/**
 * ����ϵͳ���á��ú�����ϵͳ���ú�������
 */
void do_handler_syscall (exception_frame_t * frame) {
    int func_id = frame->eax;
    int arg0 = frame->ebx;
    int arg1 = frame->ecx;
    int arg2 = frame->edx;
    int arg3 = frame->esi;

    // �����߽磬���ش���
    if (func_id < sizeof(sys_table) / sizeof(sys_table[0])) {
        // ���ȡ�ô�������Ȼ����ô���
        syscall_handler_t handler = sys_table[func_id];
        if (handler) {
            int ret = handler(arg0, arg1, arg2, arg3);
            frame->eax = ret;  // ����ϵͳ���õķ���ֵ����eax����
            return;
        }
    }

    // ��֧�ֵ�ϵͳ���ã���ӡ������Ϣ
    task_t * task = task_current();
    log_printf("task: %s, Unknown syscall: %d", task->name,  func_id);
    frame->eax = -1;  // ����ϵͳ���õķ���ֵ����eax����
}
