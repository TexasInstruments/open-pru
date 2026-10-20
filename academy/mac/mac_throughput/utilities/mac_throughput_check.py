#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
#
# File: mac_throughput_check.py
#
# Host-side golden model and record checker for the mac_throughput firmware.
#
# The firmware leaves a 0x78-byte result record at the start of the PRU's own
# data RAM. This script predicts every product in that record bit-exactly and
# checks a record read back from the device.
#
# Three independent sources must agree on the cross-check product:
#   1. this model           (host, Python integers)
#   2. the PRU MAC          (run 2, XIN from the MAC)
#   3. the PRU shift/add    (run 3, no MAC involved)
# (2) and (3) run on the same core from the same operands, so check_cross()
# catches a mis-driven MAC even if this model shares the same mistake.
#
# Usage:
#   python mac_throughput_check.py --selftest
#       model checks plus negative tests that prove the checker rejects
#       corrupted records
#   python mac_throughput_check.py --record <file> [--clock-mhz <MHz>]
#       check a record read from the device. <file> is either the raw
#       0x78-byte dump or text with 30 32-bit words (hex or decimal),
#       whitespace or comma separated, record offset 0x00 first.

import argparse
import json
import random
import sys

M32 = (1 << 32) - 1
M64 = (1 << 64) - 1

# Must match the .asg values in firmware/main.asm
SEED_A = 0x13579BDF
SEED_B = 0x2468ACE1
B_STEP = 0x11
SIGNED_STEP_A = 0x9E3779B9
SIGNED_STEP_B = 0x7F4A7C15
TIMED_ITERATIONS = 65536
XCHECK_ITERATIONS = 1024
RECORD_MAGIC = 0x4D414354  # "MACT"
RECORD_WORDS = 0x78 // 4

# Record layout: name -> word index (byte offset / 4)
FIELDS = {
    "magic": 0x00 // 4,
    "bracket_cycles": 0x04 // 4,
    "mac_timed_iterations": 0x08 // 4,
    "mac_timed_cycles": 0x0C // 4,
    "mac_timed_lo": 0x10 // 4,
    "mac_timed_hi": 0x14 // 4,
    "mac_xcheck_iterations": 0x18 // 4,
    "mac_xcheck_lo": 0x1C // 4,
    "mac_xcheck_hi": 0x20 // 4,
    "shift_add_iterations": 0x24 // 4,
    "shift_add_cycles": 0x28 // 4,
    "shift_add_lo": 0x2C // 4,
    "shift_add_hi": 0x30 // 4,
    "signed_iterations": 0x34 // 4,
    "signed_cycles": 0x38 // 4,
    "signed_lo": 0x3C // 4,
    "signed_hi": 0x40 // 4,
    "q31_iterations": 0x44 // 4,
    "q31_cycles": 0x48 // 4,
    "q31_result": 0x4C // 4,
    "signed_sum_iterations": 0x50 // 4,
    "signed_sum_lo": 0x54 // 4,
    "signed_sum_hi": 0x58 // 4,
    "signed_sum_q31": 0x5C // 4,
    "late_b_iterations": 0x60 // 4,
    "late_b_lo": 0x64 // 4,
    "late_b_hi": 0x68 // 4,
    "late_a_iterations": 0x6C // 4,
    "late_a_lo": 0x70 // 4,
    "late_a_hi": 0x74 // 4,
}


def to_signed32(value):
    value &= M32
    return value - (1 << 32) if value & (1 << 31) else value


def operands(iterations):
    """Yield (a, b) for every product of the unsigned operand recurrence."""
    a, b = SEED_A, SEED_B
    for k in range(iterations, 0, -1):
        a = (a ^ k) & M32
        b = (b + B_STEP) & M32
        yield a, b


def signed_operands(iterations):
    """Yield (a, b) for every product of the signed operand recurrence."""
    a, b = SEED_A, SEED_B
    for _ in range(iterations):
        a = (a + SIGNED_STEP_A) & M32
        b = (b + SIGNED_STEP_B) & M32
        yield a, b


def shift_add_mul64(a, b):
    """Mirror of FN_SHIFT_ADD_MUL64, step for step (not a * b)."""
    mcand_lo, mcand_hi, mplier = a & M32, 0, b & M32
    acc_lo = acc_hi = 0
    for _ in range(32):
        mask = (-(mplier & 1)) & M32
        total = acc_lo + (mcand_lo & mask)
        carry = total >> 32                       # ADD sets carry
        acc_lo = total & M32
        acc_hi = (acc_hi + (mcand_hi & mask) + carry) & M32   # ADC
        mcand_hi = ((mcand_hi << 1) | (mcand_lo >> 31)) & M32
        mcand_lo = (mcand_lo << 1) & M32
        mplier >>= 1
    return (acc_hi << 32) | acc_lo


def signed_fixup(a, b):
    """Mirror of m_mac_signed_fixup applied to the unsigned MAC product."""
    product = (a & M32) * (b & M32)
    hi, lo = product >> 32, product & M32
    hi = (hi - (((-(a >> 31)) & M32) & b)) & M32
    hi = (hi - (((-(b >> 31)) & M32) & a)) & M32
    return (hi << 32) | lo


def q31_from_product(product):
    """Mirror of m_q31_from_product: (product >> 31), truncated."""
    return (((product >> 32) << 1) | ((product & M32) >> 31)) & M32


def last(iterations, func, recurrence=operands):
    result = None
    for a, b in recurrence(iterations):
        result = func(a, b)
    return result


def signed_checksum(iterations):
    """XOR of every signed product (low, high) and Q31 result, run 6."""
    lo = hi = q31 = 0
    for a, b in signed_operands(iterations):
        product = signed_fixup(a, b)
        lo ^= product & M32
        hi ^= product >> 32
        q31 ^= q31_from_product(product)
    return lo, hi, q31


def late_product(iterations, late):
    """Product XIN returns when `late` ('a' or 'b') is written right before.

    One-instruction operand latency: the operand written by the instruction
    right before XIN still has its previous value when the product is read.
    """
    prev_a, prev_b = SEED_A, SEED_B
    result = None
    for a, b in operands(iterations):
        if late == "a":
            result = prev_a * b
        else:
            result = a * prev_b
        prev_a, prev_b = a, b
    return result & M64


def expected():
    """Every value a correct device writes to the record."""
    mac_timed = last(TIMED_ITERATIONS, lambda a, b: a * b)
    mac_xcheck = last(XCHECK_ITERATIONS, lambda a, b: a * b)
    signed = last(TIMED_ITERATIONS, signed_fixup, signed_operands)
    sum_lo, sum_hi, sum_q31 = signed_checksum(XCHECK_ITERATIONS)
    return {
        "mac_timed": mac_timed,
        "mac_xcheck": mac_xcheck,
        "shift_add": last(XCHECK_ITERATIONS, shift_add_mul64),
        "signed": signed,
        "q31": q31_from_product(signed),
        "signed_sum": (sum_hi << 32) | sum_lo,
        "signed_sum_q31": sum_q31,
        "late_b": late_product(XCHECK_ITERATIONS, "b"),
        "late_a": late_product(XCHECK_ITERATIONS, "a"),
        # what runs 6 and 7 would return with no operand latency
        "late_b_if_no_latency": mac_xcheck,
        "late_a_if_no_latency": mac_xcheck,
    }


def golden_words():
    """The record a correct device produces (cycle fields left at zero)."""
    exp = expected()
    words = [0] * RECORD_WORDS

    def put(name, value):
        words[FIELDS[name]] = value & M32

    def put64(name, value):
        put(name + "_lo", value)
        put(name + "_hi", value >> 32)

    put("magic", RECORD_MAGIC)
    put("mac_timed_iterations", TIMED_ITERATIONS)
    put64("mac_timed", exp["mac_timed"])
    put("mac_xcheck_iterations", XCHECK_ITERATIONS)
    put64("mac_xcheck", exp["mac_xcheck"])
    put("shift_add_iterations", XCHECK_ITERATIONS)
    put64("shift_add", exp["shift_add"])
    put("signed_iterations", TIMED_ITERATIONS)
    put64("signed", exp["signed"])
    put("q31_iterations", TIMED_ITERATIONS)
    put("q31_result", exp["q31"])
    put("signed_sum_iterations", XCHECK_ITERATIONS)
    put64("signed_sum", exp["signed_sum"])
    put("signed_sum_q31", exp["signed_sum_q31"])
    put("late_b_iterations", XCHECK_ITERATIONS)
    put64("late_b", exp["late_b"])
    put("late_a_iterations", XCHECK_ITERATIONS)
    put64("late_a", exp["late_a"])
    return words


def field(words, name):
    return words[FIELDS[name]]


def field64(words, name):
    return (field(words, name + "_hi") << 32) | field(words, name + "_lo")


def check_record(words):
    """Check every product and count in a record. Returns failure strings."""
    if len(words) != RECORD_WORDS:
        return [f"record has {len(words)} words, expected {RECORD_WORDS}"]
    exp = expected()
    failures = []
    if field(words, "magic") != RECORD_MAGIC:
        failures.append(f"magic 0x{field(words, 'magic'):08x} != "
                        f"0x{RECORD_MAGIC:08x} (firmware did not finish)")
    for name, want in (("mac_timed", TIMED_ITERATIONS),
                       ("mac_xcheck", XCHECK_ITERATIONS),
                       ("shift_add", XCHECK_ITERATIONS),
                       ("signed", TIMED_ITERATIONS),
                       ("q31", TIMED_ITERATIONS),
                       ("signed_sum", XCHECK_ITERATIONS),
                       ("late_b", XCHECK_ITERATIONS),
                       ("late_a", XCHECK_ITERATIONS)):
        got = field(words, name + "_iterations")
        if got != want:
            failures.append(f"{name} iterations {got} != {want}")
    for name in ("mac_timed", "mac_xcheck", "shift_add", "signed",
                 "signed_sum"):
        got = field64(words, name)
        if got != exp[name]:
            failures.append(f"{name} product 0x{got:016x} != "
                            f"0x{exp[name]:016x}")
    if field(words, "q31_result") != exp["q31"]:
        failures.append(f"q31 result 0x{field(words, 'q31_result'):08x} != "
                        f"0x{exp['q31']:08x}")
    if field(words, "signed_sum_q31") != exp["signed_sum_q31"]:
        failures.append(f"Q31 checksum 0x{field(words, 'signed_sum_q31'):08x}"
                        f" != 0x{exp['signed_sum_q31']:08x}")
    for name in ("late_b", "late_a"):
        got = field64(words, name)
        if got != exp[name]:
            note = (" (matches the no-latency product)"
                    if got == exp[name + "_if_no_latency"] else "")
            failures.append(f"{name} product 0x{got:016x} != one-instruction"
                            f" latency model 0x{exp[name]:016x}{note}")
    if field64(words, "mac_timed") == 0:
        failures.append("timed product is zero: R27:R26 not stored")
    return failures


def check_cross(words):
    """MAC run 2 must equal shift/add run 3. Independent of the model."""
    mac = field64(words, "mac_xcheck")
    soft = field64(words, "shift_add")
    if mac != soft:
        return [f"MAC 0x{mac:016x} != shift/add 0x{soft:016x} "
                f"(same core, same operands)"]
    return []


def timing(words, clock_mhz=None):
    """Cycles per product for each timed run, empty bracket subtracted."""
    bracket = field(words, "bracket_cycles")
    out = {"bracket_cycles": bracket}
    for name in ("mac_timed", "shift_add", "signed", "q31"):
        cycles = field(words, name + "_cycles") - bracket
        iterations = field(words, name + "_iterations")
        per = cycles / iterations if iterations else None
        row = {"cycles": cycles, "iterations": iterations,
               "cycles_per_product": per}
        if clock_mhz and iterations:
            row["ns_per_product"] = cycles / iterations * 1000.0 / clock_mhz
        out[name] = row
    mac = out["mac_timed"]["cycles_per_product"]
    if mac:
        out["signed_fixup_cycles"] = out["signed"]["cycles_per_product"] - mac
        out["q31_extra_cycles"] = (out["q31"]["cycles_per_product"]
                                   - out["signed"]["cycles_per_product"])
        out["shift_add_vs_mac"] = out["shift_add"]["cycles_per_product"] / mac
    return out


def selftest(vectors=100000, seed=0x26122026):
    """Model checks and negative tests. Raises AssertionError on failure."""
    rng = random.Random(seed)
    corners = [(0, 0), (1, M32), (M32, M32), (0x7FFFFFFF, 0x7FFFFFFF),
               (0x80000000, 1), (0x80000000, M32), (0x80000000, 0x80000000)]
    cases = corners + [(rng.getrandbits(32), rng.getrandbits(32))
                       for _ in range(vectors)]
    for a, b in cases:
        want = (to_signed32(a) * to_signed32(b)) & M64
        if signed_fixup(a, b) != want:
            raise AssertionError(f"signed fixup {a:#x} * {b:#x}")
        if q31_from_product(want) != ((to_signed32(a) * to_signed32(b))
                                      >> 31) & M32:
            raise AssertionError(f"q31 extraction {a:#x} * {b:#x}")
    print(f"PASS signed fixup and Q31 extraction, {len(cases)} vectors")

    for a, b in cases[:20000]:
        if shift_add_mul64(a, b) != a * b:
            raise AssertionError(f"shift/add model {a:#x} * {b:#x}")
    print("PASS shift/add model == a * b, 20000 vectors")

    exp = expected()
    if exp["mac_xcheck"] != exp["shift_add"]:
        raise AssertionError("MAC and shift/add models disagree")
    if exp["mac_xcheck"] in (exp["late_a"], exp["late_b"]):
        raise AssertionError("late-operand runs cannot show the latency")
    print(f"PASS models agree: cross-check 0x{exp['mac_xcheck']:016x}")

    signs = {(a >> 31, b >> 31) for a, b in signed_operands(XCHECK_ITERATIONS)}
    if len(signs) != 4:
        raise AssertionError(f"signed runs cover only sign pairs {signs}")
    print("PASS signed recurrence covers all four operand sign combinations")

    good = golden_words()
    if check_record(good) or check_cross(good):
        raise AssertionError("golden record fails its own checks")

    def mutate(words, name, value):
        out = list(words)
        out[FIELDS[name]] = value & M32
        return out

    negatives = [
        ("product fields zero (R27:R26 not stored)",
         check_record(mutate(mutate(good, "mac_timed_lo", 0),
                             "mac_timed_hi", 0))),
        ("single-bit flip in timed product",
         check_record(mutate(good, "mac_timed_lo",
                             field(good, "mac_timed_lo") ^ 1))),
        ("wrong timed iteration count",
         check_record(mutate(good, "mac_timed_iterations", 32768))),
        ("stale data RAM, wrong magic",
         check_record(mutate(good, "magic", 0xDEADBEEF))),
        ("shift/add product corrupted",
         check_record(mutate(good, "shift_add_lo",
                             field(good, "shift_add_lo") ^ 0x40))),
        ("MAC disagrees with shift/add",
         check_cross(mutate(good, "mac_xcheck_lo",
                            field(good, "mac_xcheck_lo") ^ 2))),
    ]
    silent = [name for name, failures in negatives if not failures]
    if silent:
        raise AssertionError("checker did not reject: " + "; ".join(silent))
    for name, failures in negatives:
        print(f"PASS negative test rejected: {name} -> {failures[0]}")


def read_record(path):
    with open(path, "rb") as handle:
        data = handle.read()
    if len(data) == RECORD_WORDS * 4:
        return [int.from_bytes(data[i:i + 4], "little")
                for i in range(0, len(data), 4)]
    tokens = data.decode("ascii").replace(",", " ").split()
    return [int(token, 0) & M32 for token in tokens]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--record", help="record dump to check")
    parser.add_argument("--clock-mhz", type=float,
                        help="PRU clock, to print ns per product")
    parser.add_argument("--golden", action="store_true",
                        help="print the expected record and exit")
    args = parser.parse_args()

    if args.golden:
        exp = expected()
        print(json.dumps({k: f"0x{v:016x}" for k, v in exp.items()},
                         indent=2))
        return 0
    if args.selftest:
        selftest()
    if args.record:
        words = read_record(args.record)
        failures = check_record(words) + check_cross(words)
        report = {"failures": failures}
        if len(words) == RECORD_WORDS:
            report["timing"] = timing(words, args.clock_mhz)
        print(json.dumps(report, indent=2))
        print("FAIL" if failures else "PASS")
        return 1 if failures else 0
    if not args.selftest:
        parser.print_help()
    return 0


if __name__ == "__main__":
    sys.exit(main())
