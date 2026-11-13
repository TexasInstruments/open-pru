# Spinlock Usage Examples

These examples show PRU0 and PRU1 sharing a resource through the PRU_ICSSG local
hardware spinlock, which a PRU reaches over the broadside interface with `XIN`/`XOUT`
(XID `0x90`, lock id `0`-`63` in `R1.b0`, status in `R1.b3`).

- `spinlock_example.asm`: assembly, using the `M_SPINLOCK_ACQUIRE` /
  `M_SPINLOCK_RELEASE` macros.
- `main.c` + `spinlock.asm`: C, calling the C-callable `spinlock_acquire()` /
  `spinlock_release()`. Both PRUs take the same lock and drive a debug GPO while they
  hold it.

For how the spinlock accelerator works, see the PRU Academy page
AM243x/AM64x PRU Academy > Hardware information > PRU Broadside Accelerators > Spinlock:

- AM64x: https://dev.ti.com/tirex/explore/node?isTheia=false&node=A__AXw28VmX5TFHmSGgvY-0qw__AM64-ACADEMY__WI1KRXP__LATEST
- AM243x: https://dev.ti.com/tirex/explore/node?isTheia=false&node=A__Ac5K7fXpNm5GbT1YWvI-8g__AM24X-ACADEMY__ZPSnq-h__LATEST

## Usage notes

Measured on LP-AM243 (ICSSG0 PRU0 and PRU1):

- The lock is not fair. A core that tries to re-acquire straight after releasing wins
  every time, because the other core's retry loop is longer than that gap. In a test
  version of `main.c` with no gap, PRU1 never got the lock. Do some work outside the lock before trying
  again, as `main.c` does with `OUTSIDE_TIME`.
- An acquire by the core that already holds the lock also returns 1. The status means
  "this core holds the lock"; it is not a nesting count, so do not take the lock
  recursively.
- An acquire is one attempt. Waiting for the lock has no upper bound: it lasts as long
  as the other core holds it, so keep critical sections short.
