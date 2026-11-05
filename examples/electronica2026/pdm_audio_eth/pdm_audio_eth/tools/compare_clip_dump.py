#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
"""Compare a CCS memory dump of the clip buffer with mcuplus/pdm_clips.h.

Board check B4 (docs/test_report.md): after pdm-eth test, save the clip
buffer from CCS (address 0x700C0000, 0x40000 bytes = 0x10000 32-bit words)
as raw binary or as TI Data (.dat, hex), then run

    python3 tools/compare_clip_dump.py DUMP

It prints "identical" (exit 0), or the number of differing bytes, the first
differences (byte offset, frame, channel) and exits 1. Only AUDIO_IN frames
(pdm-eth stream) may write the buffer, so after `test` or `capture` it must
be identical.
"""

import argparse
import re
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
HEADER = HERE.parent / "mcuplus" / "pdm_clips.h"
CLIP_BYTES = 16384 * 8 * 2
FRAME_BYTES = 16


def header_bytes(path):
    """The pdm_clip_buffer initialiser of pdm_clips.h as little-endian bytes."""
    text = path.read_text()
    body = text[text.index("pdm_clip_buffer[") :]
    body = body[body.index("{") + 1 : body.index("};")]
    values = [int(v) for v in re.findall(r"-?\d+", body)]
    return struct.pack(f"<{len(values)}h", *values)


def dump_bytes(path):
    """Raw binary, or TI Data: a '1651 <format> <addr> ...' header line, then
    one value per line. Format 9 (CCS Theia binary save) has one byte per
    line as 8 binary digits; the other formats are read as hex 32-bit words."""
    raw = path.read_bytes()
    if not raw.startswith(b"1651 "):
        return raw
    text = raw.decode("ascii")
    fields = text.splitlines()[0].split()
    values = text.split()[len(fields) :]
    if fields[1] == "9":
        return bytes(int(v, 2) for v in values)
    return b"".join(int(w, 16).to_bytes(4, "little") for w in values)


def compare(want, got, show=8):
    """(differing byte count, first `show` offsets); a short dump counts too."""
    n = min(len(want), len(got))
    diffs = [i for i in range(n) if want[i] != got[i]]
    missing = max(0, len(want) - len(got))
    return len(diffs) + missing, diffs[:show]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("dump", type=Path, help="CCS memory dump (raw binary or TI Data .dat)")
    ap.add_argument("--header", type=Path, default=HEADER)
    a = ap.parse_args(argv)
    want = header_bytes(a.header)
    got = dump_bytes(a.dump)
    if len(want) != CLIP_BYTES:
        sys.exit(f"{a.header}: {len(want)} bytes, expected {CLIP_BYTES}")
    if len(got) < CLIP_BYTES:
        print(f"dump has {len(got)} bytes, the clip buffer {CLIP_BYTES}: compared the first {len(got)}")
    count, first = compare(want, got[:CLIP_BYTES])
    if count == 0:
        print(f"identical: {CLIP_BYTES} bytes ({CLIP_BYTES // FRAME_BYTES} frames x 8 channels)")
        return 0
    print(f"DIFFERENT: {count} of {CLIP_BYTES} bytes")
    for off in first:
        f, c = off // FRAME_BYTES, (off % FRAME_BYTES) // 2
        print(f"  byte 0x{off:05X} (address 0x{0x700C0000 + off:08X}): frame {f}, channel {c}: "
              f"want 0x{want[off]:02X}, got 0x{got[off]:02X}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
