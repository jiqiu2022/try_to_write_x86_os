#include "comm/cpu_instr.h"
#include "core/task.h"
#include "tools/klib.h"
#include "os_cfg.h"
#include "cpu/cpu.h"
#include "tools/log.h"
#include "cpu/irq.h"
#include "core/memory.h"
#include "cpu/mmu.h"
#include "core/syscall.h"
static uint32_t idle_task_stack[IDLE_STACK_SIZE];	// 空闲任务堆栈

static task_manager_t task_manager;
static task_t task_table[TASK_NR];
static mutex_t task_table_mutex;

int my_strcmp(const char *str1, const char *str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}

static int tss_init(task_t* task,uint32_t entry,uint32_t esp,const char * name){

    int tss_sel=gdt_alloc_desc();
    if (tss_sel<=0){
         log_printf("alloc tss failed.\n");
        return -1;
    }
    segment_desc_set(tss_sel, (uint32_t)&task->tss, sizeof(tss_t), 
            SEG_P_PRESENT | SEG_DPL0 | SEG_TYPE_TSS);
    
    kernel_memset(&task->tss,0,sizeof(tss_t));

    uint32_t kernel_stack = memory_alloc_page();
    if (kernel_stack == 0) {
        goto tss_init_failed;
    }

    const char* aim_name ="idletask";
    int res =my_strcmp(name,aim_name);
    int code_sel, data_sel;
    if(!res){
        code_sel = KERNEL_SELECTOR_CS;
        data_sel = KERNEL_SELECTOR_DS;

    } else{
        code_sel = task_manager.app_code_sel | SEG_RPL3;
        data_sel = task_manager.app_data_sel | SEG_RPL3;
    }
    // 注意加了RP3,不然将产生段保护错误

    task->tss.eip = entry;
    task->tss.esp = esp ? esp : kernel_stack + MEM_PAGE_SIZE;
    task->tss.esp0=kernel_stack + MEM_PAGE_SIZE;
    task->tss.ss0 = KERNEL_SELECTOR_DS;
    task->tss.eip = entry;
    task->tss.eflags = EFLAGS_DEFAULT | EFLAGS_IF;
    task->tss.es = task->tss.ss = task->tss.ds 
            = task->tss.fs = task->tss.gs = data_sel;   // 暂时写死
    task->tss.cs = code_sel;    // 暂时写死
    task->tss.iomap = 0;
    task->tss_sel =tss_sel;
    uint32_t page_dir = memory_create_uvm();
    if (page_dir == 0) {
        goto tss_init_failed;
    }
    task->tss.cr3 = page_dir;
    return 0;
tss_init_failed:
    gdt_free_sel(tss_sel);

    if (kernel_stack) {
        memory_free_page(kernel_stack);
    }
    return -1;
}

void task_first_init (void) {
    void first_task_entry (void);
    extern uint8_t s_first_task[], e_first_task[];
    uint32_t copy_size = (uint32_t)(e_first_task - s_first_task);
    uint32_t alloc_size = 10 * MEM_PAGE_SIZE;
    ASSERT(copy_size < alloc_size);

    uint32_t first_start =(uint32_t)first_task_entry;
    task_init(&task_manager.first_task, "first task", first_start, first_start + alloc_size);
    task_manager.curr_task = &task_manager.first_task;


    mmu_set_page_dir(task_manager.first_task.tss.cr3);
    memory_alloc_page_for(first_start,  alloc_size, PTE_P | PTE_W | PTE_U);
    kernel_memcpy((void *)first_start, (void *)s_first_task, copy_size);
    // 写TR寄存器，指示当前运行的第一个任务
    write_tr(task_manager.first_task.tss_sel);
}
//初始化任务
int task_init (task_t *task,const char * name, uint32_t entry, uint32_t esp)
{
    ASSERT(task !=(task_t *)0);
    
    int err = tss_init(task, entry, esp,name);
    if (err < 0) {
        log_printf("init task failed.\n");
        return err;
    }
    kernel_strncpy(task->name,name,TASK_NAME_SIZE);
    task->state=TASK_CREATED;
    task->time_slice = TASK_TIME_SLICE_DEFAULT;
    task->slice_ticks = task->time_slice;
    task->sleep_ticks=0;
    task->pid=(int)task;
    task->parent=(task_t*)0;
    list_node_init(&task->all_node);
    list_node_init(&task->run_node);
    irq_state_t state = irq_enter_protection();

    task_set_ready(task);
    list_insert_last(&task_manager.task_list,&task->all_node);
    irq_leave_protection(state);

    return 0;
}
void task_switch_from_to (task_t * from, task_t * to) {
    switch_to_tss(to->tss_sel);
}
task_t * task_first_task (void) {
    return &task_manager.first_task;
}
static void idle_task_entry (void) {
    for (;;) {

        hlt();
    }
}
void task_manager_init(void){
    int sel = gdt_alloc_desc();
    segment_desc_set(sel, 0x00000000, 0xFFFFFFFF,
                     SEG_P_PRESENT | SEG_DPL3 | SEG_S_NORMAL |
                     SEG_TYPE_DATA | SEG_TYPE_RW | SEG_D);
    task_manager.app_data_sel = sel;

    sel = gdt_alloc_desc();
    segment_desc_set(sel, 0x00000000, 0xFFFFFFFF,
                     SEG_P_PRESENT | SEG_DPL3 | SEG_S_NORMAL |
                     SEG_TYPE_CODE | SEG_TYPE_RW | SEG_D);
    task_manager.app_code_sel = sel;
    list_init(&task_manager.ready_list);
    list_init(&task_manager.task_list);
    list_init(&task_manager.sleep_list);
    task_init(&task_manager.idle_task,
                "idletask",
                (uint32_t)idle_task_entry, 
                (uint32_t)(idle_task_stack + IDLE_STACK_SIZE));     // 里面的值不必要写
    task_manager.curr_task =(task_t *)0;


}

task_t * task_current (void) {
    return task_manager.curr_task;
}
//将任务加入就绪队列
void task_set_ready(task_t *task) {
     if(task==&task_manager.idle_task){
        return;
    }
    list_insert_last(&task_manager.ready_list, &task->run_node);
    task->state = TASK_READY;
}
//将任务从就绪队列中移除
void task_set_block (task_t *task) {
    if(task==&task_manager.idle_task){
        return;
    }
    list_remove(&task_manager.ready_list, &task->run_node);
}

static task_t * task_next_run (void) {

    if(!list_count(&task_manager.ready_list)){
        return &task_manager.idle_task;
    }
    // 普通任务
    list_node_t * task_node = list_first(&task_manager.ready_list);
    return list_node_parent(task_node, task_t, run_node);
}
void task_dispatch(){
    task_t * to =task_next_run();
    if(to!=task_manager.curr_task){
        task_t * from =task_manager.curr_task;
        task_manager.curr_task=to;
        
        to->state=TASK_RUNNING;
        task_switch_from_to(from,to);

    }
}
int sys_yield(void){
    irq_state_t state = irq_enter_protection();

    if (list_count(&task_manager.ready_list)>1){
        task_t * curr_task =task_current();
        task_set_block(curr_task);
        task_set_ready(curr_task);

        task_dispatch();
    }

    irq_leave_protection(state);

    return 0;
}
void task_set_wakeup (task_t *task) {
    list_remove(&task_manager.sleep_list, &task->run_node);
}
void task_time_tick (void) {
    task_t * curr_task = task_current();
    irq_state_t state = irq_enter_protection();

    // 时间片的处理
    if (--curr_task->slice_ticks == 0) {
        // 时间片用完，重新加载时间片
        // 对于空闲任务，此处减未用
        curr_task->slice_ticks = curr_task->time_slice;

        // 调整队列的位置到尾部，不用直接操作队列
        task_set_block(curr_task);
        task_set_ready(curr_task);

        
        

    }

    list_node_t *curr =list_first(&task_manager.sleep_list);
    while (curr)
    {
       list_node_t * next = list_node_next(curr);

        task_t * task = list_node_parent(curr, task_t, run_node);
        if (--task->sleep_ticks == 0) {
            // 延时时间到达，从睡眠队列中移除，送至就绪队列
            task_set_wakeup(task);
            task_set_ready(task);
        }
        curr = next;
    }
    task_dispatch();
    irq_leave_protection(state);
}

void task_set_sleep(task_t *task, uint32_t ticks) {
    if (ticks <= 0) {
        return;
    }

    task->sleep_ticks = ticks;
    task->state = TASK_SLEEP;
    list_insert_last(&task_manager.sleep_list, &task->run_node);
}
void sys_msleep(uint32_t ms){
    if(ms<OS_TICK_MS){
        ms=OS_TICK_MS;
    }

    irq_state_t state =irq_enter_protection();
    task_set_block(task_manager.curr_task);
    task_set_sleep(task_manager.curr_task, (ms + (OS_TICK_MS - 1))/ OS_TICK_MS);
    task_dispatch();

    irq_leave_protection(state);

}

int sys_getpid (void) {
    task_t * curr_task = task_current();
    return curr_task->pid;
}
task_t * alloc_task(void){
    task_t *task =(task_t *)0;
    mutex_lock(&task_table_mutex);
    for (int i = 0; i < TASK_NR; ++i) {
        task_t * curr =task_table+i;
        if (curr->name[0]=='\0'){
            task=curr;
            break;
        }
    }
    mutex_unlock(&task_table_mutex);
    return task;
}
static void free_task (task_t * task) {
    mutex_lock(&task_table_mutex);
    task->name[0] = 0;
    mutex_unlock(&task_table_mutex);
}
void task_uninit (task_t * task) {
    if (task->tss_sel) {
        gdt_free_sel(task->tss_sel);
    }

    if (task->tss.esp0) {
        memory_free_page(task->tss.esp0 - MEM_PAGE_SIZE);
    }

    if (task->tss.cr3) {
        // 没有分配空间，暂时不写
        //memory_destroy_uvm(task->tss.cr3);
    }

    kernel_memset(task, 0, sizeof(task_t));
}

int sys_fork (void) {
    task_t * parent_task=task_current();
    task_t *child_task= alloc_task();
    if (child_task == (task_t *)0) {
        log_printf("alloc task failed");
        goto fork_failed;
    }


    syscall_frame_t * frame =(syscall_frame_t *)(parent_task->tss.esp0-sizeof (syscall_frame_t));
    int err = task_init(child_task,  parent_task->name, frame->eip,
                        frame->esp + sizeof(uint32_t)*SYSCALL_PARAM_COUNT);
    if (err < 0) {
        log_printf("task init failed");
        goto fork_failed;
    }
    tss_t * tss = &child_task->tss;
    tss->eax = 0;                       // 子进程返回0
    tss->ebx = frame->ebx;
    tss->ecx = frame->ecx;
    tss->edx = frame->edx;
    tss->esi = frame->esi;
    tss->edi = frame->edi;
    tss->ebp = frame->ebp;

    tss->cs = frame->cs;
    tss->ds = frame->ds;
    tss->es = frame->es;
    tss->fs = frame->fs;
    tss->gs = frame->gs;
    tss->eflags = frame->eflags;
    child_task->parent = parent_task;


//    child_task->tss.cr3 = parent_task->tss.cr3;
    if ((child_task->tss.cr3 = memory_copy_uvm(parent_task->tss.cr3)) < 0) {
        goto fork_failed;
    }
    // 创建成功，返回子进程的pid
    return (uint32_t)child_task->pid;   // 暂时用这个
fork_failed:
    if (child_task) {
        free_task(child_task);
        task_uninit(child_task);

    }
    return -1;
}
