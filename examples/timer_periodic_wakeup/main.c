/*
 * Copyright (C) 2015 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     xtimer_examples
 * @{
 *
 * @file
 * @brief       example application for setting a periodic wakeup
 *
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 *
 * @}
 */

#include <stdio.h>
#include "xtimer.h"
#include "timex.h"

/* set interval to 1 second */
#define INTERVAL (1U * SEC_IN_USEC)

int main(void)
{
    uint32_t last_wakeup = xtimer_now();

    while(1) {
        /* note: the below conversion of a constant INTERVAL from microseconds
         * to xtimer ticks is computed at compile time and does not increase
         * code size nor incur any run time overhead.
         * The above statement has been verified by manual inspection of the
         * generated assembly code for this demo application. This is true for
         * all optimization levels from -O1 and up (including -Os, and -Og), it
         * is not precomputed at -O0, however.
         */
        xtimer_periodic_wakeup(&last_wakeup, INTERVAL);
        printf("slept until %" PRIu32 "\n", xtimer_usec_from_ticks(xtimer_now()));
    }

    return 0;
}
