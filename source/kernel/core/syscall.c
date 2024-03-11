
#include "core/syscall.h"
#include "tools/klib.h"
#include "core/task.h"
#include "tools/log.h"
#include "fs/fs.h"
#include "core/memory.h"

// ϵͳ���ô���������
typedef int (*syscall_handler_t)(uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);
int sys_print_msg (char * fmt, int arg) ;
// ϵͳ���ñ�
static const syscall_handler_t sys_table[] = {
        [SYS_msleep] = (syscall_handler_t)sys_msleep,
        [SYS_getpid] =(syscall_handler_t)sys_getpid,
        [SYS_printmsg] = (syscall_handler_t)sys_print_msg,
        [SYS_fork] = (syscall_handler_t)sys_fork,
        [SYS_execve] = (syscall_handler_t)sys_execve,
        [SYS_yield]=(syscall_handler_t)sys_yield,
        [SYS_isatty] = (syscall_handler_t)sys_isatty,
        [SYS_sbrk] = (syscall_handler_t)sys_sbrk,
        [SYS_fstat] = (syscall_handler_t)sys_fstat,
        [SYS_write] = (syscall_handler_t)sys_write,
        [SYS_close] = (syscall_handler_t)sys_close,
        [SYS_lseek] = (syscall_handler_t)sys_lseek,
};
int sys_print_msg (char * fmt, int arg) {
    log_printf(fmt, arg);
    return 0;
}
/**
 * ����ϵͳ���á��ú�����ϵͳ���ú�������
 */
void do_handler_syscall (syscall_frame_t * frame) {
    // �����߽磬���ش���
    if (frame->func_id < sizeof(sys_table) / sizeof(sys_table[0])) {
        // ���ȡ�ô�������Ȼ����ô���
        syscall_handler_t handler = sys_table[frame->func_id];
        if (handler) {
            int ret = handler(frame->arg0, frame->arg1, frame->arg2, frame->arg3);
            frame->eax=ret;
            return;
        }
    }

    // ��֧�ֵ�ϵͳ���ã���ӡ������Ϣ
    task_t * task = task_current();
    log_printf("task: %s, Unknown syscall: %d", task->name,  frame->func_id);
}
