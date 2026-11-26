#!/usr/bin/env python3
"""Write a PicoScope 7 arbitrary-waveform CSV that sends one UART frame per AWG period.

Default: "HelloWorld!" (11 bytes), 4 Mbaud, 8N1, idle high, 8 samples per bit,
200 bit times per period (110 frame bits + 90 idle bits). Set the AWG
frequency to BAUD / BITS_PER_PERIOD = 20 kHz so one bit lasts 250 ns.

PicoScope maps the CSV values -1..+1 to offset -/+ amplitude: -1 = low, +1 = high.

Usage: python3 make_awg_csv.py [output.csv] [payload]
"""
import sys

BAUD = 4_000_000
SAMPLES_PER_BIT = 8
BITS_PER_PERIOD = 200
LEAD_IDLE_BITS = 45          # idle before the frame; the rest is idle after it


def frame_bits(payload: bytes) -> list[int]:
    bits = []
    for byte in payload:
        bits.append(0)                                  # start bit
        bits += [(byte >> i) & 1 for i in range(8)]     # data, LSB first
        bits.append(1)                                  # stop bit
    return bits


def period_bits(payload: bytes) -> list[int]:
    frame = frame_bits(payload)
    tail = BITS_PER_PERIOD - LEAD_IDLE_BITS - len(frame)
    if tail < 1:
        raise SystemExit(f"frame of {len(frame)} bits does not fit in {BITS_PER_PERIOD} bits")
    return [1] * LEAD_IDLE_BITS + frame + [1] * tail


def main() -> None:
    out = sys.argv[1] if len(sys.argv) > 1 else "helloworld_4mbaud_awg.csv"
    payload = (sys.argv[2] if len(sys.argv) > 2 else "HelloWorld!").encode()
    samples = [b for b in period_bits(payload) for _ in range(SAMPLES_PER_BIT)]
    with open(out, "w", newline="\n") as f:
        f.writelines("1\n" if s else "-1\n" for s in samples)
    print(f"{out}: {len(samples)} samples, payload {payload!r}, "
          f"AWG frequency {BAUD / BITS_PER_PERIOD:g} Hz for {BAUD / 1e6:g} Mbaud")


if __name__ == "__main__":
    main()
