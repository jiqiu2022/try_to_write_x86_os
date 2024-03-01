#include "comm/cpu_instr.h"
#include "core/task.h"
#include "tools/klib.h"
#include "os_cfg.h"
#include "cpu/cpu.h"
#include "tools/log.h"
#include "cpu/irq.h"
static uint32_t idle_task_stack[IDLE_STACK_SIZE];	// 空闲任务堆栈

static task_manager_t task_manager;

static int tss_init(task_t* task,uint32_t entry,uint32_t esp){
    int tss_sel=gdt_alloc_desc();
    if (tss_sel<=0){
         log_printf("alloc tss failed.\n");
        return -1;
    }
    segment_desc_set(tss_sel, (uint32_t)&task->tss, sizeof(tss_t), 
            SEG_P_PRESENT | SEG_DPL0 | SEG_TYPE_TSS);
    
    kernel_memset(&task->tss,0,sizeof(tss_t));
    task->tss.eip = entry;
    task->tss.esp = task->tss.esp0 = esp;
    task->tss.ss0 = KERNEL_SELECTOR_DS;
    task->tss.eip = entry;
    task->tss.eflags = EFLAGS_DEFAULT | EFLAGS_IF;
    task->tss.es = task->tss.ss = task->tss.ds 
            = task->tss.fs = task->tss.gs = KERNEL_SELECTOR_DS;   // 暂时写死
    task->tss.cs = KERNEL_SELECTOR_CS;    // 暂时写死
    task->tss.iomap = 0;
    task->tss_sel =tss_sel;
    return 0;
}

void task_first_init (void) {
    task_init(&task_manager.first_task, "first task", 0, 0);

    // 写TR寄存器，指示当前运行的第一个任务
    write_tr(task_manager.first_task.tss_sel);
    task_manager.curr_task = &task_manager.first_task;
}
//初始化任务
int task_init (task_t *task,const char * name, uint32_t entry, uint32_t esp)
{
    ASSERT(task !=(task_t *)0);
    
    int err = tss_init(task, entry, esp);
    if (err < 0) {
        log_printf("init task failed.\n");
        return err;
    }
    kernel_strncpy(task->name,name,TASK_NAME_SIZE);
    task->state=TASK_CREATED;
    task->time_slice = TASK_TIME_SLICE_DEFAULT;
    task->slice_ticks = task->time_slice;
    task->sleep_ticks=0;
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
    list_init(&task_manager.ready_list);
    list_init(&task_manager.task_list);
    list_init(&task_manager.sleep_list);
    task_init(&task_manager.idle_task,
                "idle task", 
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
void task_set_sleep(task_t *task,uint32_t ticks){
    ASSERT(ticks>0);
    task->sleep_ticks=ticks;
    task->state=TASK_SLEEP;
    list_insert_last(&task_manager.sleep_list,&task->run_node);


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