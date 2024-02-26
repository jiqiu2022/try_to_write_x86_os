#ifndef IRQ_H
#define IRQ_H

void irq_init (void);
void exception_handler_unknown (void);
typedef struct _exception_frame_t {
    // 结合压栈的过程，以及pusha指令的实际压入过程
    int gs, fs, es, ds;
    int edi, esi, ebp, esp, ebx, edx, ecx, eax;
    int num;
    int error_code;
    int eip, cs, eflags;
}exception_frame_t;
#endif
