/*
 * Copyright (C) 2016 University of California, Berkeley
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     task_sched
 * @{
 *
 * @file
 * @brief       TinyOS-style task scheduler
 *
 * @author      Sam Kumar <samkumar@berkeley.edu>
 *
 * This module, built on top of the xtimer module, emulates a TinyOS-style task
 * scheduler in a single thread.
 *
 * Tasks in TinyOS execute in an event loop. When a task is "posted", it is
 * placed at the back of the event queue, unless that task is already on the
 * event queue, in which case nothing happens.
 *
 * This is slightly different from messages in RIOT's IPC mechanism in that a
 * task can only exist in the event queue in one place. This has the advantage
 * that the memory needed for the event queue is bounded by the number of
 * different tasks.
 * @}
 */

#include <mutex.h>
#include <stdint.h>
#include <xtimer.h>

struct task {
    /* All fields are for internal use by the task_sched module. */
    uint64_t _min_exec_time;
    uint64_t _req_exec_time;
    int _next;
    int _prev;
};

struct task_sched {
    /* These fields must be set before calling start_task_sched. */
    int coalesce_shift;
    int64_t max_coalesce_time_delta;
    struct task* tasks;
    int num_tasks;
    char* thread_stack;
    size_t thread_stack_size;
    char thread_priority;
    char* thread_name;
    void (*task_handler)(int task);

    /* These fields are for internal use by the task_sched module. */
    kernel_pid_t _pid;
    mutex_t _lock;
    int _first;
    xtimer_t _timer;
    bool _in_process_loop;
};

kernel_pid_t start_task_sched(struct task_sched* args);

int sched_task(struct task_sched* sched, int taskid, int64_t delay);
int cancel_task(struct task_sched* sched, int taskid);
