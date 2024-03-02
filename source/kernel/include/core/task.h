#ifndef TASK_H
#define TASK_H
#include "cpu/cpu.h"
#include "comm/types.h"
#include "tools/list.h"
#define TASK_NAME_SIZE				32			// 任务名字长度
#define TASK_TIME_SLICE_DEFAULT		10			// 时间片计数

typedef struct _task_t{
    enum {
		TASK_CREATED,
		TASK_RUNNING,
		TASK_SLEEP,
		TASK_READY,
		TASK_WAITING,
	}state;
    char name[TASK_NAME_SIZE];		// 任务名字
    int sleep_ticks;		// 睡眠时间
    int time_slice;			// 时间片
	int slice_ticks;		// 递减时间片计数
    tss_t tss;
    uint16_t tss_sel;		// tss选择子
    list_node_t run_node;		// 运行相关结点
	list_node_t all_node;		// 所有队列结点
    list_node_t wait_node;
}task_t;

typedef struct _task_manager_t
{
    task_t * curr_task;
    

    list_t ready_list;
    list_t task_list;
    list_t sleep_list;
    
    task_t first_task;
    task_t idle_task;			// 空闲任务

}task_manager_t;



void task_switch_from_to (task_t * from, task_t * to);

int task_init (task_t *task,const char * name, uint32_t entry, uint32_t esp);
void task_manager_init (void);
void task_first_init (void);
task_t * task_first_task (void);
void task_set_ready(task_t *task);
int sys_yield(void);
void task_time_tick (void);
void sys_msleep(uint32_t ms);
void task_dispatch(void);
task_t * task_current (void);
void task_set_block (task_t *task);
#endif