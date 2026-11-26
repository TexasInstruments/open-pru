"""Generate the development-flow image (SVG) for the electronica 2026 i2c_io demo."""
import sys, textwrap
from xml.sax.saxutils import escape

W, H = 1600, 980
FONT = "DejaVu Sans, Verdana, sans-serif"
MONO = "DejaVu Sans Mono, monospace"
INK, MUTED, LINE = "#1f2933", "#52606d", "#cbd2d9"

STAGES = [
    dict(n="1", title="Simulate", color="#2563a8",
         quote=None,
         lead="PRU assembly in TI's pru-simulator",
         items=["source/i2c_tca9538_running_led.asm",
                "Cycle-accurate PRU0 at 200 MHz",
                "IO device: TCA9538 I2C model at address 0x23",
                "Bit-banged I2C at 1 Mbit/s: 100-cycle half bits",
                "Checked: CONFIG = 0x00, LED byte walks 0x01 to 0x80"]),
    dict(n="2", title="Port to open-pru", color="#7b4bb7",
         quote="Move the project … to open-pru … It should run on real target AM243 LP using CCS.",
         lead="MCU+ SDK project for AM243x LP",
         items=["electronica2026/i2c_io_expander/i2c_io",
                "R5F FreeRTOS loads ICSSG0 PRU0 only",
                "SysConfig: PRU 200 MHz, GPO0 = SCL, GPO1 = SDA",
                "Silicon change: push-pull SDA, ACK not sampled",
                "671 of 671 SCL edges identical to the simulator"]),
    dict(n="3", title="Run on LP-AM243", color="#c2410c",
         quote="option 2, add 100 ms pause",
         lead="Built and loaded from CCS 21",
         items=["SCL on BoosterPack pin 33, SDA on pin 32",
                "100 ms idle after each LED write",
                "Running light visible, one frame per scope capture",
                "Simulated step: 100.03 ms"]),
    dict(n="4", title="Trace with scope", color="#0f766e",
         quote="I am only using a pico-scope to show the I2C at 1 MHz.",
         lead="PicoScope 3416E MSO on USB 3",
         items=["Digital inputs: D0 = SCL, D1 = SDA",
                "PicoScope 7 I2C decoder, Fast-mode Plus",
                "Pulse-width trigger on the idle bus",
                "Built-in MCP server (beta) on localhost"]),
    dict(n="5", title="Visualize in Rust", color="#b91c1c",
         quote="Would like to use RUST for this if possible.",
         lead="LED window next to PicoScope 7",
         items=["electronica2026/i2c_io_expander/visualizer",
                "Reads the decode via MCP every 50 ms",
                "Draws the TCA9538 board with 8 LEDs",
                "Keeps up with the scope screen",
                "46 tests, design, plan and final review"]),
]

PATH = ["ICSSG0 PRU0", "I2C bus, 1 Mbit/s", "PicoScope 3416E MSO", "PicoScope 7 decode",
        "MCP on localhost", "Rust LED window"]

out = []
def add(s): out.append(s)

def text(x, y, s, size=15, fill=INK, weight="normal", anchor="start", family=FONT, style="normal"):
    add(f'<text x="{x}" y="{y}" font-family="{family}" font-size="{size}" fill="{fill}" '
        f'font-weight="{weight}" font-style="{style}" text-anchor="{anchor}">{escape(s)}</text>')

def chars(width_px, size, bold=False, mono=False):
    """Approximate characters per line for DejaVu at this size."""
    k = 0.61 if mono else (0.64 if bold else 0.56)
    return int(width_px / (size * k))

def wrap_lines(s, n, path=False):
    if not path:
        return textwrap.wrap(s, n)
    lines, cur = [], ""
    for part in s.replace("/", "/\0").split("\0"):
        if cur and len(cur) + len(part) > n:
            lines.append(cur); cur = ""
        cur += part
    return lines + [cur]

def wrapped(x, y, s, width_chars, size, lh, path=False, **kw):
    lines = wrap_lines(s, width_chars, path)
    for i, line in enumerate(lines):
        text(x, y + i * lh, line, size=size, **kw)
    return y + len(lines) * lh

add(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">')
add('<defs><marker id="arr" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto">'
    f'<path d="M0,0 L10,5 L0,10 z" fill="{MUTED}"/></marker>'
    '<radialGradient id="glow"><stop offset="0" stop-color="#ff3b3b" stop-opacity="0.75"/>'
    '<stop offset="1" stop-color="#ff3b3b" stop-opacity="0"/></radialGradient></defs>')
add(f'<rect width="{W}" height="{H}" fill="#f7f8fa"/>')

# Title
text(40, 62, "From prompt to booth demo: PRU I2C running LED", size=34, weight="bold")
text(40, 96, "electronica 2026 · AM243x LaunchPad · ICSSG0 PRU0 bit-bangs I2C writes for a TCA9538 IO expander",
     size=18, fill=MUTED)

# Stage cards
CW, CH, GAP, X0, Y0 = 272, 620, 40, 40, 135
for i, st in enumerate(STAGES):
    x = X0 + i * (CW + GAP)
    c = st["color"]
    add(f'<rect x="{x}" y="{Y0}" width="{CW}" height="{CH}" rx="14" fill="#ffffff" stroke="{LINE}" stroke-width="1.5"/>')
    add(f'<path d="M{x},{Y0+14} a14,14 0 0 1 14,-14 h{CW-28} a14,14 0 0 1 14,14 v48 h-{CW} z" fill="{c}"/>')
    add(f'<circle cx="{x+30}" cy="{Y0+31}" r="16" fill="#ffffff"/>')
    text(x + 30, Y0 + 37, st["n"], size=18, weight="bold", fill=c, anchor="middle")
    text(x + 54, Y0 + 38, st["title"], size=19, weight="bold", fill="#ffffff")
    y = Y0 + 92
    if st["quote"]:
        qn = chars(CW - 44, 15)
        qh = 22 * len(textwrap.wrap("“" + st["quote"] + "”", qn)) + 18
        add(f'<rect x="{x+14}" y="{y-20}" width="{CW-28}" height="{qh}" rx="8" fill="{c}" fill-opacity="0.08"/>')
        add(f'<rect x="{x+14}" y="{y-20}" width="4" height="{qh}" fill="{c}"/>')
        text(x + 26, y - 4 + 0, "Prompt", size=11, weight="bold", fill=c)
        y = wrapped(x + 26, y + 14, "“" + st["quote"] + "”", qn, 15, 22, style="italic", fill=INK)
        y += 22
    else:
        add(f'<rect x="{x+14}" y="{y-20}" width="{CW-28}" height="40" rx="8" fill="{c}" fill-opacity="0.08"/>')
        add(f'<rect x="{x+14}" y="{y-20}" width="4" height="40" fill="{c}"/>')
        text(x + 26, y + 5, "Prompt in an earlier session", size=14, style="italic", fill=MUTED)
        y += 52
    y = wrapped(x + 18, y, st["lead"], chars(CW - 32, 17, bold=True), 17, 23, weight="bold")
    y += 10
    for it in st["items"]:
        add(f'<circle cx="{x+24}" cy="{y-5}" r="3.5" fill="{c}"/>')
        mono = "/" in it and " " not in it
        y = wrapped(x + 36, y, it, chars(CW - 50, 14, mono=True) if mono else chars(CW - 50, 15),
                    14 if mono else 15, 21, path=mono, family=MONO if mono else FONT, fill=INK)
        y += 9
    if i < len(STAGES) - 1:
        ax = x + CW + 4
        add(f'<line x1="{ax}" y1="{Y0+CH/2}" x2="{ax+GAP-8}" y2="{Y0+CH/2}" stroke="{MUTED}" '
            'stroke-width="2.5" marker-end="url(#arr)"/>')

# LED strip inside card 5
x5 = X0 + 4 * (CW + GAP)
by = Y0 + CH - 92
add(f'<rect x="{x5+16}" y="{by}" width="{CW-32}" height="74" rx="8" fill="#0b4d2c" stroke="#0e6b3c" stroke-width="2"/>')
for k in range(8):
    cx = x5 + 16 + (CW - 32) * (k + 0.5) / 8
    if k == 3:
        add(f'<circle cx="{cx}" cy="{by+30}" r="20" fill="url(#glow)"/>')
        add(f'<circle cx="{cx}" cy="{by+30}" r="10" fill="#ff3b3b" stroke="#ff8888" stroke-width="1.5"/>')
    else:
        add(f'<circle cx="{cx}" cy="{by+30}" r="10" fill="#3a1010" stroke="#552222" stroke-width="1.5"/>')
    text(cx, by + 62, f"P{k}", size=11, fill="#b8d8c4", anchor="middle", family=MONO)

# Simulator mini waveform inside card 1
x1 = X0
wy = Y0 + CH - 92
add(f'<rect x="{x1+16}" y="{wy}" width="{CW-32}" height="74" rx="8" fill="#0f172a"/>')
text(x1 + 26, wy + 22, "SCL", size=11, fill="#93c5fd", family=MONO)
text(x1 + 26, wy + 56, "SDA", size=11, fill="#fcd34d", family=MONO)
sx0, sx1 = x1 + 58, x1 + CW - 26
d, xx, hi = [], sx0, True
step = (sx1 - sx0) / 18
d.append(f"M{sx0},{wy+14}")
for k in range(18):
    yv = wy + 14 if k % 2 == 0 else wy + 30
    d.append(f"L{sx0+k*step},{yv} L{sx0+(k+1)*step},{yv}")
add(f'<path d="{" ".join(d)}" fill="none" stroke="#93c5fd" stroke-width="2"/>')
bits = [1, 0, 0, 0, 1, 1, 0, 1, 0]
d = []
for k, b in enumerate(bits):
    yv = wy + 48 if b else wy + 64
    d.append(f"{'M' if k == 0 else 'L'}{sx0+k*2*step},{yv} L{sx0+(k+1)*2*step},{yv}")
add(f'<path d="{" ".join(d)}" fill="none" stroke="#fcd34d" stroke-width="2"/>')

def panel(i):
    x = X0 + i * (CW + GAP)
    y = Y0 + CH - 92
    add(f'<rect x="{x+16}" y="{y}" width="{CW-32}" height="74" rx="8" fill="#0f172a"/>')
    return x, y

# Card 2: what the R5F sets up
x2, y2 = panel(1)
for k, (a, b) in enumerate([("ICSSG0 PRU0", "200 MHz"), ("GPO0", "SCL  BP 33"), ("GPO1", "SDA  BP 32")]):
    text(x2 + 28, y2 + 21 + k * 20, a, size=12, fill="#c4b5fd", family=MONO)
    text(x2 + 140, y2 + 21 + k * 20, b, size=12, fill="#e5e7eb", family=MONO)

# Card 3: one frame, then 100 ms idle
x3, y3 = panel(2)
text(x3 + 28, y3 + 22, "frame", size=11, fill="#fdba74", family=MONO)
text(x3 + 120, y3 + 22, "idle 100 ms", size=11, fill="#9ca3af", family=MONO)
add(f'<rect x="{x3+28}" y="{y3+32}" width="40" height="22" fill="#fb923c"/>')
add(f'<line x1="{x3+68}" y1="{y3+43}" x2="{x3+CW-40}" y2="{y3+43}" stroke="#9ca3af" stroke-width="2" stroke-dasharray="4 4"/>')
add(f'<rect x="{x3+CW-40}" y="{y3+32}" width="10" height="22" fill="#fb923c"/>')
text(x3 + 28, y3 + 68, "29 µs", size=11, fill="#fdba74", family=MONO)

# Card 4: PicoScope 7 decode of one frame
x4, y4 = panel(3)
text(x4 + 28, y4 + 22, "I2C decode", size=11, fill="#5eead4", family=MONO)
fx = x4 + 28
for lab in ["S", "23 W", "01", "08", "P"]:
    w = 18 if len(lab) == 1 else 9 * len(lab) + 14
    add(f'<rect x="{fx}" y="{y4+34}" width="{w}" height="24" rx="4" fill="#134e4a" stroke="#5eead4"/>')
    text(fx + w / 2, y4 + 51, lab, size=12, fill="#ccfbf1", family=MONO, anchor="middle")
    fx += w + 6

# Booth data path
py = Y0 + CH + 40
add(f'<rect x="40" y="{py}" width="{W-80}" height="120" rx="14" fill="#ffffff" stroke="{LINE}" stroke-width="1.5"/>')
text(64, py + 34, "At the booth: data path", size=19, weight="bold")
pw, pg = 210, 42
px0 = 64
for k, name in enumerate(PATH):
    px = px0 + k * (pw + pg)
    add(f'<rect x="{px}" y="{py+52}" width="{pw}" height="46" rx="23" fill="#eef2f6" stroke="{LINE}"/>')
    text(px + pw / 2, py + 81, name, size=15, anchor="middle", weight="bold")
    if k < len(PATH) - 1:
        add(f'<line x1="{px+pw+4}" y1="{py+75}" x2="{px+pw+pg-6}" y2="{py+75}" stroke="{MUTED}" '
            'stroke-width="2.5" marker-end="url(#arr)"/>')

text(40, H - 28, "Built with Claude Code (Claude Opus 5.5): pru-assembly skill for lint, build and simulation; "
     "design → plan → test-first build for the Rust app. Steps 2-5: 2026-11-02.", size=14, fill=MUTED)
add("</svg>")
open(sys.argv[1], "w").write("\n".join(out) + "\n")
