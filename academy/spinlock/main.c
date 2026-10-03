/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2024-2026 Texas Instruments Incorporated - http://www.ti.com/
 *
 * Spinlock Example - C Version (PRU_ICSSG local hardware spinlock)
 *
 * Demonstrates mutual exclusion between PRU0 and PRU1 using the PRU-local
 * hardware spinlock, accessed over the broadside interface (XIN/XOUT). The
 * low-level acquire/release are implemented in spinlock.asm; this file shows
 * the usage pattern.
 *
 * Each PRU repeatedly takes the same spinlock, drives a debug GPO high while it
 * holds the lock, waits a (per-core different) number of cycles, then releases
 * and drives the debug GPO low. Probing the two debug pins shows that the two
 * cores never hold the lock at the same time.
 */

#include <stdint.h>

/*
 * Acquire/release are defined in spinlock.asm (separately linked). They use
 * broadside XID 0x90, which is the spinlock in the *local* PRU subsystem; a
 * separate function would be needed for a spinlock in a different subsystem.
 *
 * spinlock_acquire() is a single non-blocking attempt: it returns 1 if the
 * lock was acquired, 0 otherwise. The spin loop lives here in C.
 */
uint8_t spinlock_acquire(uint8_t flag_id);
void    spinlock_release(uint8_t flag_id);

/* Spinlock flag id (valid range 0-63). Both PRUs must use the same id. */
#define SPINLOCK_FLAG 11

/* R30 drives the PRU GPO signals; use a debug pin to scope lock ownership. */
/* TODO: pinmux the debug signals (see the GPIO lab) and update the shift to */
/*       match your board. PRU0 is selected via -DPRU0 in the makefile.      */
volatile register uint32_t __R30;
#if PRU0
#define DEBUG_PIN_SHIFT     4       /* PRU0 debug pin */
#else
#define DEBUG_PIN_SHIFT     5       /* PRU1 debug pin */
#endif

/* Hold the lock for a different time per core so contention is observable. */
#if PRU0
#define HOLD_SPINLOCK_TIME  1000    /* PRU0 holds for 1000 cycles */
#else
#define HOLD_SPINLOCK_TIME  2000    /* PRU1 holds for 2000 cycles */
#endif

void main(void)
{
    uint8_t result = 0;

    /* Start with all GPO signals low. */
    __R30 = 0x00000000;

    while (1) {
        /* Spin until we own the lock (acquire returns 1 on success). */
        result = 0;
        do {
            result = spinlock_acquire(SPINLOCK_FLAG);
        } while (result != 1);

        /* Critical section: we hold the lock. Drive the debug pin high. */
        __R30 |= (1 << DEBUG_PIN_SHIFT);

        __delay_cycles(HOLD_SPINLOCK_TIME);

        /* Release the lock, then drop the debug pin. */
        spinlock_release(SPINLOCK_FLAG);
        __R30 &= ~(1 << DEBUG_PIN_SHIFT);
    }

    /* Not reached because of the while(1) loop above. */
    __halt();
}
