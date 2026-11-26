#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
"""Generate docs/signal_flow.svg (signal flow and clocks of pdm_audio_eth).

Usage: python3 tools/gen_signal_flow.py docs/signal_flow.svg
PNG:   chromium --headless --hide-scrollbars --window-size=1920,1212 \
           --screenshot=$PWD/docs/signal_flow.png file://$PWD/docs/signal_flow.svg
"""
import sys
from xml.sax.saxutils import escape

W, H = 1920, 1212
out = []
def o(s): out.append(s)

# ---- columns and rows -------------------------------------------------
CW, GAP, X0 = 232, 34, 46
def cx(c): return X0 + c * (CW + GAP)          # left edge of column c
R1Y, R2Y, RH = 196, 556, 300                    # row tops, row height

DOM = {   # name: (band fill, header colour)
    "pc":   ("#eaf1fb", "#2c6fb7"),
    "eth":  ("#f4ecfa", "#7d3c98"),
    "g1":   ("#e7f5ec", "#1e7b45"),
    "mem":  ("#f1f2f4", "#5f6b7a"),
    "g0":   ("#fdf1e4", "#c0601a"),
    "pin":  ("#fdecea", "#c0392b"),
}

o(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" '
  'font-family="Segoe UI, Ubuntu, Helvetica, Arial, sans-serif">')
o('''<defs>
<marker id="a" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="#2f3b4c"/></marker>
<marker id="ad" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="#8e44ad"/></marker>
<marker id="ac" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="#c0601a"/></marker>
<style>
.t{font-size:13px;fill:#1f2933}
.tb{font-size:13px;fill:#1f2933;font-weight:700}
.ts{font-size:12px;fill:#3e4c59}
.h{font-size:14.5px;font-weight:700;fill:#fff}
.hs{font-size:11.5px;fill:#f1f4f8}
.bh{font-size:15px;font-weight:700;fill:#fff}
.bs{font-size:12px;fill:#fff}
.lab{font-size:12px;fill:#2f3b4c;font-weight:600}
.labd{font-size:12px;fill:#8e44ad;font-weight:600}
.labc{font-size:12px;fill:#c0601a;font-weight:600}
.sec{font-size:19px;font-weight:700;fill:#1f2933}
.num{font-size:13px;font-weight:700}
</style></defs>''')
o(f'<rect width="{W}" height="{H}" fill="#fff"/>')
o('<text x="46" y="44" font-size="26" font-weight="700" fill="#1f2933">'
  'PDM audio over Gigabit Ethernet: signal flow and clocks, WAV file to 8 PDM microphones and back to the GUI</text>')
o('<text x="46" y="72" class="ts" font-size="14">LP-AM243 Rev A, ICSSG0 + ICSSG1 at 333.33 MHz, '
  'PRU firmware only in the data path (the R5F configures and idles). Each column is one clock domain. '
  'Numbers: design.md, firmware headers, test_report.md (board run 2026-11-05).</text>')

# ---- clock-domain bands --------------------------------------------------
bands = [  # (first col, last col, dom, title, clock line 1, clock line 2)
    (0, 1, "pc",  "PC: Ubuntu 26.04, Rust pdm-eth", "CLOCK_MONOTONIC; display 60 Hz", "follows the board clock (pacer)"),
    (2, 2, "eth", "Ethernet cable", "1000BASE-T FD, RGMII2", "125 MHz DDR = 8 ns per byte"),
    (3, 3, "g1",  "ICSSG1", "core 333.33 MHz (3 ns)", "IEP +3 per cycle = ns timestamps"),
    (4, 4, "mem", "Shared memory", "no clock of its own:", "hand-off between domains"),
    (5, 6, "g0",  "ICSSG0: the audio sample clock", "core 333.33 MHz -> PDM 3.072 MHz", "-> 16 kHz PCM (master clock)"),
]
for c0, c1, d, title, l1, l2 in bands:
    fill, hdr = DOM[d]
    x = cx(c0) - 12
    w = cx(c1) + CW + 12 - x
    o(f'<rect x="{x}" y="92" width="{w}" height="{R2Y + RH - 92 + 14}" rx="12" fill="{fill}" stroke="{hdr}" stroke-opacity="0.35"/>')
    o(f'<rect x="{x}" y="92" width="{w}" height="86" rx="12" fill="{hdr}"/>')
    o(f'<text x="{x + 14}" y="118" class="bh">{escape(title)}</text>')
    o(f'<text x="{x + 14}" y="141" class="bs">{escape(l1)}</text>')
    o(f'<text x="{x + 14}" y="160" class="bs">{escape(l2)}</text>')

# ---- blocks ----------------------------------------------------------------
def block(c, y, d, num, title, sub, lines, h=RH):
    fill, hdr = DOM[d]
    x = cx(c)
    o(f'<rect x="{x}" y="{y}" width="{CW}" height="{h}" rx="8" fill="#ffffff" stroke="{hdr}" stroke-width="2"/>')
    o(f'<rect x="{x}" y="{y}" width="{CW}" height="44" rx="8" fill="{hdr}"/>'
      f'<rect x="{x}" y="{y + 30}" width="{CW}" height="14" fill="{hdr}"/>')
    o(f'<circle cx="{x + 17}" cy="{y + 22}" r="11" fill="#ffffff"/>'
      f'<text x="{x + 17}" y="{y + 26.5}" text-anchor="middle" class="num" fill="{hdr}">{num}</text>')
    o(f'<text x="{x + 34}" y="{y + 19}" class="h">{escape(title)}</text>')
    o(f'<text x="{x + 34}" y="{y + 36}" class="hs">{escape(sub)}</text>')
    ty = y + 64
    for ln in lines:
        cls = "t"
        if ln.startswith("*"):
            cls, ln = "tb", ln[1:]
        elif ln.startswith("~"):
            cls, ln = "ts", ln[1:]
        if ln:
            o(f'<text x="{x + 12}" y="{ty}" class="{cls}">{escape(ln)}</text>')
        ty += 17.5

# Row 1: forward path, PC -> board -> pins
block(0, R1Y, "pc", 1, "WAV sources", "pc_app wavin.rs, sources.rs", [
    "*one mono source per channel",
    "booth clip, speech, music,",
    "speech + music, silence, any",
    "WAV file or a capture set",
    "",
    "converted on load:",
    "resample to 16 kHz, mono,",
    "int16, peak scaled to 0.5 FS",
    "(near full scale would wrap",
    "the RTU1 modulators)",
    "~each channel loops on its length",
])
block(1, R1Y, "pc", 2, "Pacer + AUDIO_IN TX", "pacing.rs, link.rs (AF_PACKET)", [
    "*next index = RTU1 position",
    "*                  + 0.5 s lead",
    "(8000 frames ahead, ring 1.024 s)",
    "",
    "AUDIO_IN frame: 1062 B,",
    "64 frames x 8 ch x int16 =",
    "1024 B + target sample index",
    "*250 frames/s = 256 KB/s",
    "",
    "raw socket on enp129s0",
    "~counts underrun / blocked / restart",
])
block(2, R1Y, "eth", 3, "PC -> board", "raw Ethernet II, EtherType 0x88B5", [
    "RTL8125 (2.5 GbE, 1 GbE link)",
    "direct cable to J18,",
    "DP83869 PHY address 15",
    "",
    "1062 B frame + preamble + IFG",
    "*= about 8.7 us on the wire",
    "every 4 ms",
    "",
    "use: 2 Mbit/s of 1000",
    "~(throughput test: 984 Mbit/s,",
    "~0 lost frames)",
])
block(3, R1Y, "g1", 4, "ICSSG1 PRU1", "eth_rx.asm: receiver", [
    "RX_L2 FIFO, 32-byte banks:",
    "*1 bank per 256 ns = 85 cycles",
    "filter EtherType / board MAC",
    "",
    "AUDIO_IN: each bank stored",
    "while the frame arrives,",
    "one SBBO (posted) to MSRAM",
    "CLIP_ADDR + (idx & 16383) x 16",
    "",
    "pings: RX IEP time to PRU0",
    "~counters -> ICSSG1 shared RAM",
])
block(4, R1Y, "mem", 5, "MSRAM clip ring", "0x700C0000, = PC input ring", [
    "*16384 frames x 8 ch x int16",
    "*= 256 KB = 1.024 s",
    "frame = 16 B, size 2^14,",
    "so PRU1 wraps with an AND",
    "",
    "PC writes 0.5 s ahead,",
    "RTU1 reads behind",
    "",
    "pre-filled with the booth",
    "clips: plays without a PC",
    "~(no RTU1 change for the PC)",
])
block(5, R1Y, "g0", 6, "ICSSG0 RTU1", "rtu1_mod.asm: 8 modulators", [
    "*fetch 1 frame per 192 PDM",
    "*periods (16 kHz, held: ZOH)",
    "8 x 2nd-order sigma-delta,",
    "1 bit per channel per period",
    "",
    "batches of 28 periods",
    "(9.11 us, 3038 cycles) to",
    "IPC scratchpad XFR ID 15",
    "RTU1 worst case 58 % busy",
    "",
    "~publishes RTU1_FRAME_POS, LOOPS",
])
block(6, R1Y, "g0", 7, "ICSSG0 PRU1", "pru1_pdm.asm: PDM driver", [
    "*cycle-exact PDM clock",
    "*3.072 MHz = 108 + 73/144",
    "*cycles per period",
    "(71 x 108 + 73 x 109 per 144)",
    "",
    "clock on GPO0 (BP.11),",
    "8 data pins, 1 bit per",
    "channel per clock period",
    "",
    "takes 1 batch per 28 periods,",
    "~counts UNDERRUN (late batch)",
])

# Row 2: return path, pins -> PCM -> PC (drawn right to left)
block(6, R2Y, "pin", 8, "8 PDM microphones", "emulated: 9 jumper wires", [
    "*1 clock + 8 data wires",
    "PRU1 GPO -> PRU0 GPI",
    "clock GPO0 BP.11 ->",
    "GPI16 BP.7 = SD8_CLK,",
    "shared by all 8 channels",
    "",
    "the same signals as 8 PDM",
    "MEMS microphones: 3.072 MHz,",
    "1-bit data per channel",
    "",
    "~real mics replace the source only",
])
block(5, R2Y, "g0", 9, "ICSSG0 PRU0", "pru0_rx.asm: SD decoder", [
    "8 sigma-delta channels on",
    "the shared SD8_CLK",
    "*SINC3, OSR 96 -> 32 kHz",
    "*63-tap FIR, / 2 -> 16 kHz",
    "20 Hz DC blocker,",
    "24-bit saturation in int32",
    "",
    "1 PCM frame = 8 ch x int32",
    "every 62.5 us",
    "8-ch worst case 39 % busy",
    "~counts SD_OVF",
])
block(4, R2Y, "mem", 10, "ICSSG0 shared RAM", "0x30010000: header + halves", [
    "*ping-pong halves A / B:",
    "*64 frames x 8 ch x int32",
    "*= 2 KB = 4 ms each",
    "",
    "HALF_SEQ++ when a half is",
    "full: 250 times per second",
    "",
    "header: HALF_SEQ, RTU1 pos,",
    "UNDERRUN, SD_OVF, PCM_COUNT",
    "",
    "~4 ms before a half is reused",
])
block(3, R2Y, "g1", 11, "ICSSG1 PRU0", "eth_tx.asm: transmitter", [
    "polls HALF_SEQ (global",
    "0x30010018), IEP timestamp,",
    "reads the header status",
    "",
    "copies 2 x 1 KB in 64-byte",
    "bursts to its DRAM0, sends",
    "*2 AUDIO_OUT of 1118 B",
    "*per half = 500 frames/s",
    "TX_L2 FIFO, 64 B per XOUT",
    "",
    "~also PONGs and test frames",
])
block(2, R2Y, "eth", 12, "board -> PC", "AUDIO_OUT, to the last PC MAC", [
    "1118 B: header, status",
    "block, first sample index,",
    "32 frames x 8 ch x int32",
    "",
    "*512 KB/s (4 Mbit/s of 1000)",
    "about 9.1 us per frame on",
    "the wire, 2 frames per 4 ms",
    "",
    "MII_RT TX_IPG 0x28 (109 ns",
    "measured gap)",
    "~(throughput test: 976 Mbit/s)",
])
block(1, R2Y, "pc", 13, "AUDIO_OUT RX", "rx.rs: reconstruction", [
    "*placed by sample index:",
    "*(HALF_SEQ - 1) x 64",
    "*                + part x 32",
    "not by arrival time",
    "",
    "lost / dup / resync counted,",
    "gap filled with silence",
    "",
    "status block -> pacer",
    "IEP time -> board clock ppm",
    "~(+10.9 ppm vs PC, measured)",
])
block(0, R2Y, "pc", 14, "GUI + capture", "ui.rs, spectro.rs (egui, wgpu)", [
    "*IN pane: samples as sent",
    "*OUT pane: board PCM",
    "difference strip (dB per bin)",
    "",
    "STFT 256 Hann, hop 128:",
    "8 ms = 125 columns/s",
    "0-8 kHz, -90 to 0 dB",
    "*60 fps measured (need 30)",
    "",
    "capture: WAV per channel,",
    "~16 kHz 24-bit, in and out",
])

# ---- arrows ------------------------------------------------------------------
def line(x1, y1, x2, y2, cls="a", dash=False, colour="#2f3b4c", w=2.6):
    d = ' stroke-dasharray="7 5"' if dash else ''
    o(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{colour}" stroke-width="{w}"{d} marker-end="url(#{cls})"/>')

def path(dpath, cls="a", dash=False, colour="#2f3b4c", w=2.6):
    d = ' stroke-dasharray="7 5"' if dash else ''
    o(f'<path d="{dpath}" fill="none" stroke="{colour}" stroke-width="{w}"{d} marker-end="url(#{cls})"/>')

ay1 = R1Y + RH / 2
ay2 = R2Y + RH / 2
for c in range(6):                                   # row 1, left to right
    line(cx(c) + CW + 2, ay1, cx(c + 1) - 3, ay1)
for c in range(6, 0, -1):                            # row 2, right to left
    line(cx(c) - 2, ay2, cx(c - 1) + CW + 3, ay2)
# PRU1 pins down to the microphones
x7 = cx(6) + CW / 2
line(x7, R1Y + RH + 2, x7, R2Y - 3, w=3.2, colour="#c0392b")

mid = (R1Y + RH + R2Y) / 2
o(f'<text x="{x7 - 10}" y="{mid - 6}" text-anchor="end" class="lab" style="fill:#c0392b">3.072 MHz clock</text>')
o(f'<text x="{x7 - 10}" y="{mid + 10}" text-anchor="end" class="lab" style="fill:#c0392b">+ 8 x 1-bit data</text>')
# status block feedback: RX -> pacer (board is the clock master)
x2 = cx(1) + CW / 2
line(x2, R2Y - 2, x2, R1Y + RH + 4, cls="ad", dash=True, colour="#8e44ad")
o(f'<text x="{x2 - 10}" y="{mid - 6}" text-anchor="end" class="labd">status: RTU1 position</text>')
o(f'<text x="{x2 - 10}" y="{mid + 10}" text-anchor="end" class="labd">paces AUDIO_IN</text>')
# input samples to the GUI IN pane
x1 = cx(0) + CW / 2
line(x1, R1Y + RH + 2, x1, R2Y - 4, cls="ad", dash=True, colour="#8e44ad")
o(f'<text x="{x1 + 10}" y="{mid - 6}" class="labd">input as sent,</text>')
o(f'<text x="{x1 + 10}" y="{mid + 10}" class="labd">aligned by index</text>')
# RTU1 position into the ICSSG0 header
xr = cx(5) + 40
xs = cx(4) + CW - 40
path(f"M{xr},{R1Y + RH + 2} L{xr},{mid} L{xs},{mid} L{xs},{R2Y - 4}", cls="ac", dash=True, colour="#c0601a")
o(f'<text x="{xr + 8}" y="{mid - 8}" class="labc">RTU1_FRAME_POS, LOOPS</text>')


# ---- clock and rate chain ------------------------------------------------------
cy0 = R2Y + RH + 46
o(f'<text x="46" y="{cy0}" class="sec">Clock and rate chain: one sample clock, the ICSSG0 core clock; everything else follows it</text>')

def chain(y, items, colour, label):
    o(f'<text x="46" y="{y + 30}" class="tb" style="fill:{colour}">{escape(label)}</text>')
    x = 250
    bw, gw = 166, 64
    for i, (big, small) in enumerate(items):
        o(f'<rect x="{x}" y="{y}" width="{bw}" height="50" rx="7" fill="#ffffff" stroke="{colour}" stroke-width="2"/>')
        o(f'<text x="{x + bw / 2}" y="{y + 21}" text-anchor="middle" class="tb">{escape(big)}</text>')
        o(f'<text x="{x + bw / 2}" y="{y + 39}" text-anchor="middle" class="ts">{escape(small)}</text>')
        x += bw + gw
    return [250 + i * (bw + gw) for i in range(len(items))], bw, gw

def ops(y, xs, bw, gw, labels, colour):
    for i, lab in enumerate(labels):
        xa = xs[i] + bw + 3
        line(xa, y + 25, xa + gw - 7, y + 25, colour=colour, w=2)
        top, bot = lab
        o(f'<text x="{xa + gw / 2 - 2}" y="{y + 15}" text-anchor="middle" class="lab" style="fill:{colour}">{escape(top)}</text>')
        if bot:
            o(f'<text x="{xa + gw / 2 - 2}" y="{y + 45}" text-anchor="middle" class="ts">{escape(bot)}</text>')

ya = cy0 + 22
xs, bw, gw = chain(ya, [
    ("333.33 MHz", "ICSSG0 core, 3 ns"),
    ("3.072 MHz", "PDM clock (PRU1)"),
    ("32 kHz", "SINC3 output"),
    ("16 kHz", "PCM frame, 62.5 us"),
    ("250 Hz", "ping-pong half, 4 ms"),
    ("500 frames/s", "AUDIO_OUT to the PC"),
    ("125 columns/s", "spectrogram, 8 ms"),
], "#c0601a", "Return (decode)")
ops(ya, xs, bw, gw, [("/108.51", "PRU1"), ("/96", "SINC3"), ("/2", "FIR"), ("/64", "frames"),
                     ("x2", "1 KB parts"), ("/128", "hop")], "#c0601a")

yb = ya + 82
xs, bw, gw = chain(yb, [
    ("250 frames/s", "AUDIO_IN from the PC"),
    ("16 kHz", "clip frames in MSRAM"),
    ("3.072 MHz", "1 bit per ch (RTU1)"),
    ("109.7 kHz", "batch of 28 periods"),
    ("3.072 MHz", "pins, cycle-exact"),
    ("1.024 s", "ring = 16384 frames"),
    ("0.5 s", "PC lead = 8000 frames"),
], "#2c6fb7", "Forward (modulate)")
ops(yb, xs[:5], bw, gw, [("x64", "frames"), ("x192", "ZOH"), ("/28", "SPAD"), ("x28", "PRU1"), ("", "")], "#2c6fb7")
# the last two boxes are sizes, not a chain step: replace the 5th arrow with a separator
o(f'<rect x="{xs[4] + bw + 1}" y="{yb - 4}" width="{gw - 2}" height="58" fill="#ffffff"/>')
o(f'<line x1="{xs[4] + bw + gw / 2}" y1="{yb - 2}" x2="{xs[4] + bw + gw / 2}" y2="{yb + 52}" stroke="#9aa5b1" stroke-width="1.5" stroke-dasharray="4 4"/>')

# ---- key numbers ---------------------------------------------------------------
yk = yb + 92
o(f'<rect x="46" y="{yk}" width="{W - 92}" height="{H - yk - 22}" rx="10" fill="#f7f9fb" stroke="#c9d2dc"/>')
notes = [
    ("Who sets the time.", "PRU1 (ICSSG0) is the only free-running audio clock. RTU1 consumes the MSRAM ring at its rate and "
     "reports its position in every AUDIO_OUT status block; the PC pacer keeps 0.5 s ahead of that position, never of its own clock."),
    ("Alignment.", "Both directions carry sample indices. The PC places output by (HALF_SEQ - 1) x 64 + part x 32 and the GUI lines "
     "the IN and OUT panes up by index, so PC clock drift (+10.9 ppm measured) and network jitter never shift audio."),
    ("Measured (2026-11-05).", "Board audio latency 1.08 ms, constant (RTU1 takes a sample -> its AUDIO_OUT leaves the board, best case). "
     "500 / 250 frames/s out / in, 0 lost, UNDERRUN and SD_OVF 0, GUI 60 fps, board PONG turnaround 0.3 to 32 us."),
]
ty = yk + 26
for head, body in notes:
    o(f'<text x="64" y="{ty}" class="t"><tspan class="tb">{escape(head)}</tspan> {escape(body)}</text>')
    ty += 24

o('</svg>')
open(sys.argv[1], "w").write("\n".join(out) + "\n")
