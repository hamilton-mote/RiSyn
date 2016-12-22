/*
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @brief       Condition variable for thread synchronization
 * @ingroup     core
 * @{
 *
 * @file
 * @brief       RIOT synchronization API
 *
 * @author      Sam Kumar <samkumar@berkeley.edu>
 */

#ifndef CONDITION_H_
#define CONDITION_H_

#include <stddef.h>

#include "list.h"
#include "mutex.h"

#ifdef __cplusplus
 extern "C" {
#endif

/**
 * @brief Condition variable structure. Must never be modified by the user.
 * This condition variable has Mesa-semantics, so any waiting thread should
 * the condition in a loop.
 */
typedef struct {
    /**
     * @brief   The process waiting queue of the condition variable.
     * @internal
     */
    list_node_t queue;
} condition_t;

/**
 * @brief Static initializer for mutex_t.
 * @details This initializer is preferable to mutex_init().
 */
#define COND_INIT { { NULL } }

/**
 * @brief Initializes a condition variable.
 * @details For initialization of variables use CONDITON_INIT instead.
 *          Only use the function call for dynamically allocated mutexes.
 * @param[out] cond    pre-allocated condition structure, must not be NULL.
 */
static inline void cond_init(condition_t* cond)
{
    cond->queue.next = NULL;
}

/**
 * @brief Waits on a condition.
 *
 * @param[in] condition Condition variable to wait on.
 * @param[in] mutex Mutex object held by the current thread.
 */
void cond_wait(condition_t* cond, mutex_t* mutex);

/**
 * @brief Wakes up one thread waiting on the condition variable. The thread is
 * marked as runnable and will only be scheduled later at the scheduler's whim,
 * so the thread should re-check the condition and wait again if it is not
 * fulfilled.
 *
 * @param[in] cond Condition variable to signal.
 */
void cond_signal(condition_t* cond);

/**
 * @brief Wakes up all threads waiting on the condition variable. They are
 * marked as runnable and will only be scheduled later at the scheduler's whim,
 * so they should re-check the condition and wait again if it is not fulfilled.
 *
 * @param[in] mutex Mutex object to unlock, must not be NULL.
 */
void cond_broadcast(condition_t* cond);


#ifdef __cplusplus
}
#endif

#endif
