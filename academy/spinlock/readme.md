# Spinlock Usage Examples

This lab shows how a PRU core uses the **PRU_ICSSG local hardware spinlock** to get
mutual exclusion over a resource shared between PRU0, PRU1 and/or the host CPU.

## How the PRU-local spinlock works

The PRU_ICSSG includes a hardware spinlock accelerator with **64 ownership flags**
(lock id `0`–`63`). A PRU core reaches it over the **broadside interface** using the
`XIN`/`XOUT` instructions — there is no memory-mapped load/store on the critical path,
so acquire/release are only a couple of cycles.

- The broadside device id (XID) **`0x90` (= 144)** selects the spinlock in the
  **local** PRU subsystem. A different XID is needed to reach a spinlock in another
  PRU subsystem.
- The **lock id** to operate on is placed in **`R1.b0`**.
- **Acquire** (`XIN 0x90, &R1.b3, 1`): the accelerator returns the acquisition status
  in **`R1.b3`** — **bit 0 = 1** means the lock is now owned by this core, **bit 0 = 0**
  means it is held by someone else, so retry. A single `XIN` is one attempt; busy-wait
  until you get a 1.
- **Release** (`XOUT 0x90, &R1.b3, 1`): with the same lock id still in `R1.b0`, frees
  the lock.

Consequences:

- You acquire by **spinning on the status bit**, not by writing a value.
- A spinlock provides **mutual exclusion only** — it is not a mailbox. To pass data or
  events between cores, guard a shared buffer with the lock and signal separately (ICSS
  shared RAM + a system event / interrupt; see *Signalling* below).
- Only the owner should release a lock, and it must release promptly.

## Files

- `spinlock.asm`: C-callable `spinlock_acquire()` / `spinlock_release()` built on the
  broadside `XIN`/`XOUT` spinlock primitives.
- `main.c`: C example — both PRUs take the same lock, drive a debug GPO while holding it,
  and release. The spin loop lives in C; the per-attempt acquire is in `spinlock.asm`.
- `spinlock_example.asm`: pure-assembly example using `M_SPINLOCK_ACQUIRE` /
  `M_SPINLOCK_RELEASE` macros.

## How to Build

Adapt the silicon version to your device (`--silicon_version=3` for AM62x/AM64x/AM243x
class ICSSG).

### Assembly only

```bash
clpru --silicon_version=3 spinlock_example.asm
```

### C + assembly

Compile both and link together; select the core with `-DPRU0` for the PRU0 build:

```bash
clpru --silicon_version=3 -DPRU0=1 main.c spinlock.asm   # PRU0 firmware
clpru --silicon_version=3            main.c spinlock.asm   # PRU1 firmware
```

## Acquire / release pattern

### Assembly (macros)

```asm
INT_SPIN_XID    .set    144     ; XID 0x90 - local PRU spinlock
SPINLOCK_ID     .set    0       ; lock 0-63

M_SPINLOCK_ACQUIRE  .macro
    .newblock
    LDI     R1.b0, SPINLOCK_ID
$1:
    XIN     INT_SPIN_XID, &R1.b3, 1     ; status in R1.b3
    QBBC    $1, R1.b3, 0                ; bit 0 clear -> retry
    .endm

M_SPINLOCK_RELEASE  .macro
    XOUT    INT_SPIN_XID, &R1.b3, 1
    .endm
```

### C (calling the assembly helpers)

```c
uint8_t spinlock_acquire(uint8_t flag_id);   /* returns 1 if acquired */
void    spinlock_release(uint8_t flag_id);

/* Acquire: spin until acquire returns 1. */
while (spinlock_acquire(SPINLOCK_FLAG) != 1) { /* busy-wait */ }

/* ... critical section ... */

spinlock_release(SPINLOCK_FLAG);
```

## Signalling (what NOT to do with a spinlock)

A spinlock only provides mutual exclusion. To pass data or events between the PRU and
another core, guard a shared buffer with the lock and signal separately:

```c
while (spinlock_acquire(0) != 1) { }
shared->command = value;   /* protected write to ICSS shared RAM */
spinlock_release(0);
/* then raise a PRU system event (R31) to interrupt the other core */
```

## References

- Device-specific Technical Reference Manual (TRM) — PRU_ICSSG broadside interface and
  spinlock accelerator.
- *PRU Assembly Instruction User Guide* (SPRUIJ2) — `XIN` / `XOUT` / `XCHG`.
- *PRU Optimizing C/C++ Compiler User's Guide* — function calling conventions.
- E2E reference thread: "PRU accessing HW-Spinlocks with __xin __xout".
