//
// Created by jiqiu2021 on 2024-03-02.
//
#include "ipc/mutex.h"
#include "cpu/irq.h"
void mutex_init (mutex_t * mutex){
    list_init(&mutex->wait_list);
    mutex->owner=(task_t *)0;
    mutex->locked_count = 0;
}
/// 获取锁
/// \param mutex
void mutex_lock(mutex_t * mutex){
    irq_state_t  irqState =irq_enter_protection();
    task_t * curr =task_current();
    if(mutex->locked_count==0){
        mutex->locked_count=1;
        mutex->owner=curr;
        //curr!=(task_t *)0 这里竟然出现了curr和owner同时为0
    } else if (mutex->owner==curr&&curr!=(task_t *)0){
        mutex->locked_count++;
    } else{
        task_t *curr =task_current();
        list_insert_last(&mutex->wait_list, &curr->wait_node);
        task_set_block(curr);
        task_dispatch();
    }
    irq_leave_protection(irqState);

}
///释放锁
/// \param mutex
void mutex_unlock(mutex_t *mutex){
    irq_state_t  irqState =irq_enter_protection();

    task_t  *curr =task_current();
    if(mutex->owner==curr){
        if (--mutex->locked_count==0){
            mutex->owner=(task_t *)0;

            if(list_count(&mutex->wait_list)){
                list_node_t *task_node = list_remove_first(&mutex->wait_list);
                task_t *task = list_node_parent(task_node, task_t, wait_node);
                mutex->locked_count = 1;
                mutex->owner = task;
                task_set_ready(task);
                task_dispatch();
            }
        }

    }
    irq_leave_protection(irqState);

}