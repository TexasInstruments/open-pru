# How to run the demo: 8-channel PDM audio over Gigabit Ethernet

Booth and lab guide for `pdm_audio_eth` on an LP-AM243 with a Linux PC. It
takes you from a cold board to the live window, with every command. Background
and design: [design.md](design.md); board results: [test_report.md](test_report.md);
how the demo was built: [development_flow.md](development_flow.md).

**What the visitor sees:** eight audio channels go from the PC into the board
over Gigabit Ethernet, through a real PDM (pulse-density modulation) chain on
the PRUs: eight sigma-delta modulators, PDM clock and data pins, eight SD
decoders. They then come back to the PC, which shows input and output
spectrograms side by side, in real time. The same chain in the pru-simulator
runs at about 11 minutes per second of audio.

## Contents

1. [What you need](#1-what-you-need)
2. [One-time PC setup](#2-one-time-pc-setup)
3. [Board setup](#3-board-setup)
4. [Build the firmware](#4-build-the-firmware)
5. [Load and start the board](#5-load-and-start-the-board)
6. [Check the Ethernet link](#6-check-the-ethernet-link)
7. [Run the demo](#7-run-the-demo)
8. [Booth talking points](#8-booth-talking-points)
9. [Troubleshooting](#9-troubleshooting)
10. [Switching to another demo](#10-switching-to-another-demo)

Paths below assume the open-pru checkout at `~/GitHub/open-pru`. Set two shell
variables once per terminal; the commands use them:

```
export DEMO=~/GitHub/open-pru/examples/electronica2026/pdm_audio_eth
export PDM=$DEMO/pc_app/target/release/pdm-eth
```

**Quick start** (PC set up once as in 2, board wired and running as in 3 to
5): open a terminal anywhere and type

```
pdm-eth-demo
```

It builds the PC application if needed, restores its network permissions,
prepares the NIC, checks the link and opens the live window (7.1). The
variables above are only needed for the step-by-step commands.

## 1. What you need

| Item | Used here |
| ---- | --------- |
| Board | LP-AM243 Rev A (an E3 board works too, see 3), 5 V / 3 A USB-C supply on J10, USB cable to the on-board XDS110 (JTAG and console) |
| Jumpers | 9 short female-female jumper wires (PDM clock + 8 data lines) |
| Ethernet | one cable from the PC straight to the LP's RJ45 **J18** (ICSSG1 port 1, RGMII2); no switch |
| PC | Ubuntu 26.04, a Gigabit NIC dedicated to the board (here a Realtek RTL8125, `enp129s0`) |
| Board tools | MCU+ SDK for AM243x 12.01.00.33, CCS 21.0.1 (ti-cgt-pru 2.3.3, ti-cgt-armllvm 5.1.1.LTS, SysConfig 1.28.1 come with them) |
| PC tools | Rust 1.99 (`rustup`), `ethtool` |

If your NIC has another name, replace `enp129s0` everywhere (`ip -br link`
lists them).

## 2. One-time PC setup

### 2.1 Build the PC application

```
cd $DEMO/pc_app
cargo build --release
sudo setcap cap_net_raw,cap_net_admin+ep $PDM
```

`pdm-eth` sends raw Ethernet frames, so it needs the two capabilities. **Every
`cargo build` removes them: run the `setcap` line again after each build.**
Check with `getcap $PDM` (it must print `cap_net_admin,cap_net_raw=ep`).

`pdm-eth` is not on your `PATH`; use `$PDM` or the full path. A symlink makes
it shorter: `ln -s $PDM ~/.local/bin/pdm-eth` (it follows new builds; `setcap`
is still needed after each).

**Install the one-step launcher** (`pc_app/pdm-eth-demo.sh`, see 7.1):

```
ln -sf $DEMO/pc_app/pdm-eth-demo.sh ~/.local/bin/pdm-eth-demo
```

`~/.local/bin` is on Ubuntu's default `PATH` (open a new terminal if the
directory did not exist before). The launcher finds the demo through the
symlink, so it keeps working after `git pull` and new builds.

Without a board you can try everything against the built-in emulator:
`$PDM gui --emulator`, or `pdm-eth-demo --emulator`.

### 2.2 Keep Ubuntu off the demo cable

```
sudo nmcli device set enp129s0 managed no
```

Otherwise NetworkManager sends DHCP and IPv6 traffic on the cable. The board
drops and counts it (RX_FOREIGN), but it is noise. `pdm-eth-demo` does this
for you when the NIC is still managed.

### 2.3 CPU isolation (for the throughput test)

The audio itself is light (about 4.5 Mbit/s from the board, 2.1 Mbit/s to it).
The 64-byte throughput test pushes about 1.5 million frames per second, though.
At that rate the NIC drops frames (`rx_missed`) when its interrupt and the
`pdm-eth` network thread share a CPU or land on a busy one. On the test PC two
CPUs are isolated: CPU 6 for the NIC interrupt, CPU 7 for `pdm-eth`, which
finds the free isolated CPU itself.

`/etc/default/grub.d/99-pdm-eth-isolcpus.cfg`:

```
# pdm-eth: isolate CPUs 6 and 7 (remove this file and run update-grub to undo)
GRUB_CMDLINE_LINUX_DEFAULT="$GRUB_CMDLINE_LINUX_DEFAULT isolcpus=domain,managed_irq,6-7 nohz_full=6-7 rcu_nocbs=6-7 irqaffinity=0-5,8-19"
```

`/usr/local/sbin/pdm-eth-irq.sh` (`chmod 755`). The Realtek driver creates its
interrupt only when the interface is opened, so the script waits for it:

```
#!/bin/sh
# pdm-eth: put every interrupt of enp129s0 on CPU 6.
# r8169 requests its IRQ only when the interface is opened, so wait for it.
i=0
while [ $i -lt 300 ]; do
    irqs=$(awk '$NF == "enp129s0" { sub(":", "", $1); print $1 }' /proc/interrupts)
    if [ -n "$irqs" ]; then
        for n in $irqs; do echo 6 > "/proc/irq/$n/smp_affinity_list"; done
        echo "enp129s0 IRQ $irqs -> CPU 6"
        exit 0
    fi
    sleep 1; i=$((i + 1))
done
echo "enp129s0 IRQ not found after 300 s" >&2
exit 1
```

`/etc/systemd/system/pdm-eth-irq.service`:

```
[Unit]
Description=pdm-eth: enp129s0 interrupt on CPU 6
After=sys-subsystem-net-devices-enp129s0.device network-pre.target
Wants=sys-subsystem-net-devices-enp129s0.device

[Service]
Type=simple
ExecStart=/usr/local/sbin/pdm-eth-irq.sh
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
```

Then:

```
sudo systemctl daemon-reload
sudo systemctl enable --now pdm-eth-irq.service
sudo update-grub
```

**Before `update-grub`, check that every kernel in `/boot` has an initrd**
(`ls /boot/vmlinuz-* /boot/initrd.img-*`). On the test PC an unattended
kernel update without a working NVIDIA DKMS build left a kernel without an
initrd. `update-grub` made it the default, and the next boot panicked. That
PC pins its working kernel with `/etc/default/grub.d/98-pin-kernel-38.cfg`
(`GRUB_DEFAULT` set to the menu id of 7.0.0-38).

After the reboot, check:

```
cat /sys/devices/system/cpu/isolated                          # 6-7
for n in $(awk '$NF=="enp129s0"{sub(":","",$1);print $1}' /proc/interrupts); do
    cat /proc/irq/$n/effective_affinity_list; done            # 6
```

## 3. Board setup

**Before power-up:**

- **SW6 in the default position, LD17 off.** The SD5 to SD7 data paths and
  PRU1_GPO16 go through switches that SW6 controls. This applies to Rev A
  only: **an E3 board has no SW6, LD17 or TMUX1574 switches** (Rev A added
  them for the BP servo board), and there these paths go straight to the
  header. The 18 pins of the jumper table below are routed the same on E3 and
  Rev A (schematics PROC109E3 and PROC109A, SPRR433D). The demo runs on an E3
  with this table (2026-11-07).
- **No BoosterPack plugged in.** The jumpers use the BoosterPack header pins.
- **Boot switches set to NO BOOT** (MCU+ SDK docs, "EVM Setup", AM243X-LP).
  The R5F application sets up the SoC from SysConfig; no init script is
  needed.

**The nine jumpers** connect PRU1 (PDM clock and data out) to PRU0 (SD
inputs). The pin numbers are the BoosterPack numbers printed on the LP
headers. Keep the wires short: the clock runs at 3.072 MHz.

| Signal | From PRU1 output | To PRU0 input | SD channel |
| ------ | ---------------- | ------------- | ---------- |
| PDM clock | GPO0, **BP.11** | GPI16, **BP.7** | SD8_CLK, shared |
| ch0 | GPO1, **BP.67** | GPI1, **BP.32** | SD0_D |
| ch1 | GPO2, **BP.68** | GPI3, **BP.19** | SD1_D |
| ch2 | GPO6, **BP.69** | GPI5, **BP.13** | SD2_D |
| ch3 | GPO11, **BP.70** | GPI7, **BP.44** | SD3_D |
| ch4 | GPO12, **BP.72** | GPI11, **BP.43** | SD5_D (SD4 goes to the Ethernet PHY) |
| ch5 | GPO13, **BP.71** | GPI13, **BP.15** | SD6_D |
| ch6 | GPO15, **BP.59** | GPI15, **BP.12** | SD7_D |
| ch7 | GPO16, **BP.57** | GPI17, **BP.18** | SD8_D |

Ground for a scope probe: BP.20, BP.22, BP.60 or BP.62. The jumpers need no
ground wire.

**Ethernet:** cable from the PC's NIC to **J18**. (The other RJ45 is ICSSG1
port 0, not used.)

**USB:** power on J10, XDS110 USB to the PC.

## 4. Build the firmware

From the project directory, never from the open-pru root (a root build
dirties `pru_i2s`):

```
cd $DEMO/pdm_audio_eth
make DEVICE=am243x \
     MCU_PLUS_SDK_PATH=$HOME/ti/mcu_plus_sdk_am243x_12_01_00_33 \
     SYSCFG_NODE=$HOME/ti/ccs2101/ccs/tools/node/node
```

This builds the five PRU firmwares (ICSSG0 RTU1, PRU1, PRU0; ICSSG1 PRU1,
PRU0) and the R5F application, which embeds them:
`mcuplus/am243x-lp/r5fss0-0_freertos/ti-arm-clang/pdm_audio_eth.release.out`.
The `SYSCFG_NODE` and SDK overrides are only needed when `imports.mak` does
not point at your tools.

**On a new PC:** a fresh checkout has no `imports.mak`, and `make` stops with
`../../../../imports.mak: No such file or directory`. Copy
`imports.mak.default` at the open-pru root to `imports.mak` (git ignores it)
and set the paths of your tools: `CCS_PATH` (for example
`$(TOOLS_PATH)/ccs2101/ccs`), `CGT_TI_PRU_PATH`
(`$(CCS_PATH)/tools/compiler/ti-cgt_pru_2.3.3.LTS`), `CGT_TI_ARM_CLANG_PATH`
(`$(CCS_PATH)/tools/compiler/ti-cgt-armllvm_5.1.1.LTS`), `MCU_PLUS_SDK_PATH`
(`$(TOOLS_PATH)/mcu_plus_sdk_am243x_12_01_00_33`) and `SYSCFG_PATH`
(`$(TOOLS_PATH)/sysconfig_1.28.1`). The defaults in `imports.mak.default`
(ccs1280, SysConfig 1.21.2) are older tools.

**Or with CCS:** File > Import Projects, import the six project specs:

```
firmware/am243x-lp/icss_g0_rtu_pru1_fw/ti-pru-cgt/example.projectspec
firmware/am243x-lp/icss_g0_pru1_fw/ti-pru-cgt/example.projectspec
firmware/am243x-lp/icss_g0_pru0_fw/ti-pru-cgt/example.projectspec
firmware/am243x-lp/icss_g1_pru1_fw/ti-pru-cgt/example.projectspec
firmware/am243x-lp/icss_g1_pru0_fw/ti-pru-cgt/example.projectspec
mcuplus/am243x-lp/r5fss0-0_freertos/ti-arm-clang/example.projectspec
```

Build the five PRU projects first, then the R5F project
`pdm_audio_eth_am243x-lp_r5fss0-0_freertos_ti-arm-clang`.

**CCS copies the sources at import.** After a change in the repo, re-import
the project **and build it**. Re-importing alone does not build; a reload then
loads the previous image. Check the time stamp of
`~/workspace_ccstheia/<project>/Debug/*.out` before you load.

## 5. Load and start the board

1. Power the board (J10). Start CCS, launch the LP target configuration (MCU+
   SDK docs, "Create Target Configuration"; do not bypass the ICSS cores).
2. Connect to **`MAIN_Cortex_R5_0_0`**, load the R5F `.out` (from CCS or the
   make build above) and run. The R5F loads and starts all five PRU cores
   itself.
3. Console: the XDS110 UART, 115200 baud, 8N1 (or the CCS console). Expected:

   ```
   PDM audio eth: link 1000 FD on ICSSG1 port 1 after 3610 ms, board MAC 02:00:5d:4d:00:01
   PDM audio: ICSSG0 RTU1, PRU1, PRU0 running; 8 channels, clips at 0x700c0000 (16384 frames), PCM header at 0x30010000
   ```

   The link time varies (about 3.6 s). `link DOWN ... after 10000 ms`
   means no cable or no link partner; the demo starts anyway and works once the
   link comes up.

**Restart rule:** always restart all cores through the R5F (run the R5F
project again). Never restart a single PRU core from the CCS register view:
RTU1 and PRU1 must start together.

**Reset:** SW1 (MCU PORZ) resets the SoC. Then reconnect CCS (JTAG does not
survive the reset) and run the R5F again.

**Power-cycle when in doubt.** SW1 and reloading the R5F do not always bring
the board back to a clean state. On an E3 board (2026-11-07), after other demos
and several reloads, two failures survived SW1 and only went away after
removing the J10 supply for about 10 s:

- RTU1 never ran a batch: header BATCH_COUNT, RTU1_FRAME_POS, RTU1_LOOPS and
  HALF_SEQ all 0, the board sent no audio frames, although the console printed
  both lines;
- every core ran and every counter rose, but all eight channels decoded to
  exactly 0. PRU0's SD block saw the data inputs stuck at 0 (channel block at
  0x30000900: DC estimate at +0x1C about -2^23, delay line all 0xFFF94000 =
  -96^3/2), while a logic analyser showed real PDM on BP.67 and BP.32.

The cause was not found. Pin mux, pads, GPCFG0 and the SD set-up read as
designed. So: before a booth day, after another demo, or when the output stays
silent, **unplug J10 for 10 s**, then reconnect CCS to `MAIN_Cortex_R5_0_0`
only, load and run.

## 6. Check the Ethernet link

```
ethtool enp129s0 | grep -E "Speed|Duplex|Link detected"     # 1000Mb/s, Full, yes
$PDM test --iface enp129s0
```

Expected (board run 2026-11-05):

```
PASS link: 1000/full
PASS ping: 10000/10000, rtt min/mean/p99/max 18/32/172/4006 us, board turnaround 0.30-31.33 us
PASS to board: 243968/243968 frames, 984.4 Mbit/s, 0 sequence gaps
PASS from board: 243823/243823 frames, 976.0 Mbit/s, 0 sequence gaps
PASS to board 64 B: 1793024/1793024 frames, 0.598 Mframes/s (40 % of line rate), 286.8 Mbit/s, 0 sequence gaps
PASS from board 64 B: 4464286/4464286 frames, 1.459 Mframes/s (98 % of line rate), 700.3 Mbit/s, 0 sequence gaps
```

A single ping of about 4 ms is the PC, not the board: the board turnaround
stays below 33 µs. The first run after power-up takes a few seconds longer
(link training).

**The 64-byte test depends on the PC's NIC.** On a PC with a Realtek
RTL8111/8168 (r8169 driver, `enp3s0`, receive ring at most 256 frames,
2026-11-07), `from board 64 B` FAILs: 1600086/4464286 frames, 46152 counted in
`rx_missed`, the rest not counted by the NIC. A single ping may also be lost
(9999/10000). The full-size tests pass at 984 Mbit/s with 0 gaps there, and the
demo itself works: the audio uses full-size frames at a few Mbit/s. Section
2.3 (CPU isolation) only helps the 64-byte case. 98 % of line rate needs a
NIC like the RTL8125. Do not run `test` while the GUI is open: both use the
same NIC, and `test` then reports losses.

## 7. Run the demo

### 7.1 The live window

From any directory (launcher installed as in 2.1):

```
pdm-eth-demo
```

The launcher runs these steps, then opens the window:

| Step | Same as | `sudo` |
| ---- | ------- | ------ |
| `cargo build --release` (a no-op when nothing changed) | 2.1 | no |
| `setcap` on `pdm-eth`, only if a build removed the capabilities | 2.1 | yes, then |
| `nmcli device set <NIC> managed no`, only if still managed | 2.2 | yes, then |
| `ip link set <NIC> up`, only if down | 9 | yes, then |
| prints the link speed; warns if there is no link or not 1000 Mb/s | 6 | no |
| `pdm-eth gui --iface <NIC> [your arguments]` | below | no |

Without a link it warns ("cable in J18? R5F running?") and opens the window
anyway; it works once the link comes up. It does not run `pdm-eth test`: run
that by hand (6) when you want the throughput numbers.

| Command | Does |
| ------- | ---- |
| `pdm-eth-demo` | live window on `enp129s0` |
| `pdm-eth-demo --emulator` | no board: the built-in emulator (skips the NIC steps) |
| `pdm-eth-demo --ch0 a.wav ...` | extra arguments go to `pdm-eth gui` (7.4) |
| `PDM_IFACE=enp5s0 pdm-eth-demo` | another NIC (`--iface` is refused: use the variable) |
| `PDM_NO_BUILD=1 pdm-eth-demo` | skip the build step |
| `pdm-eth-demo --help` | this summary |

The same window by hand, after 2.1, 2.2 and 6:

```
$PDM gui --iface enp129s0
```

- **Top pane:** the selected channel's input as the PC sends it. **Middle:**
  the board's output after the PDM round trip. **Strip:** input minus output
  in dB. Scales: 0 to 8 kHz up the left, colour bar on the right, the last
  10 s along the bottom.
- **Channel buttons** (toolbar) choose the channel on screen. All eight run
  all the time, so switching is instant and keeps history.
- **Side panel:** sources (7.2), then the live numbers. Spectrogram fps (56
  to 60 with vsync), frames/s out (500) and in (250), board audio latency
  (about 1.08 ms, read on a noise or speech channel), buffered ahead (0.50 s),
  lost / dup / resync (0), UNDERRUN / SD_OVF (0), tx_late (0), PC kernel
  drops (0).
- At the start every channel is on *booth clip* and the window streams them
  to the board right away, so the IN pane shows the selected channel's clip
  next to the OUT pane. This also puts the real booth clips back into the
  board's clip buffer, which the last session's sources had overwritten (a
  window closed with ch0 on *silence* used to leave ch0 silent for the next
  one). Until 2026-11-07 the window streamed nothing until the first choice,
  and the IN pane said "no input stream".

### 7.2 Choose audio per channel

The **Sources** table has one dropdown per channel:

| Entry | Plays |
| ----- | ----- |
| booth clip | the board's own clip of that channel (ch0 speech, ch1/ch2 chirps, ch3 to ch7 tones of 1000, 3000, 500, 2000, 5000 Hz) |
| speech | 30 s of speech (LibriVox, public domain) |
| music | 30 s of Chopin, Nocturne Op. 9 No. 2 (Musopen, CC0) |
| speech + music | the two mixed, music 12 dB below the speech |
| silence | nothing |
| open WAV file... | any WAV file (any rate, stereo mixed down), through the desktop's file dialog |

- A choice plays **live**, in the stream that runs since the start, and
  channels you did not change keep their booth clip.
- **A new source starts at its first sample. You hear it about 0.5 s later:**
  the PC keeps the board's input buffer 0.5 s ahead.
- Each file is scaled to a peak of 0.5 FS. Each channel loops on its own
  length.
- While the file dialog is open the window does not redraw. The audio keeps
  running.
- Good demo: ch0 *speech + music*, ch1 *music*, then switch between ch0, ch1
  and a tone channel (ch3).

### 7.3 Synchronized capture sets (multi-microphone recordings)

**open capture set...** (above the table) multi-selects up to 8 mono WAV files
of one sample rate, for example a real 8-microphone recording:

- sorted by **file name** onto ch0, ch1, ... (name them `mic0.wav` ...
  `mic7.wav`);
- all channels **start on the same sample**;
- **trimmed to the shortest file**, so they loop together; the panel shows
  the length and the samples cut from each file;
- **one gain for the set:** its loudest sample maps to 0.5 FS, so the level
  differences between the microphones are kept;
- stereo files, mixed sample rates or more than 8 files are refused with an
  error naming the file. Nothing changes then.

The eight output channels are time-aligned: the decimation filter's even/odd
stagger is compensated in PRU0 (odd-channel coefficient table T_odd, see the
pdm_audio readme). Firmware before 2026-11-07 put the even channels half a
16 kHz sample (31.25 us) behind the odd ones.

### 7.4 Command line: start with files

```
pdm-eth-demo --ch0 a.wav --ch1 b.wav ... --ch7 h.wav
$PDM gui --iface enp129s0 --ch0 a.wav --ch1 b.wav ... --ch7 h.wav   # same, by hand
```

Files given here are loaded before the stream starts. Channels without a file
play their booth clip.

### 7.5 Record the board output

```
$PDM capture --iface enp129s0 --seconds 600 --out ~/pdm_capture
python3 $DEMO/pdm_audio_eth/tools/analyze_capture.py ~/pdm_capture
```

`capture` writes `out_ch0.wav` to `out_ch7.wav` (16 kHz, 24-bit) and prints
lost / duplicates / resyncs (expected 0) and the board counters. The analysis
fits the five booth tones: frequency, level, residual (about -85 to
-90 dB). It also checks that every 1.024 s clip loop repeats.

### 7.6 Stream files into the board and record both sides

```
python3 $DEMO/pdm_audio_eth/tools/make_b8_inputs.py ~/b8_in        # 8 test signals, 70 s
$PDM stream --iface enp129s0 \
     --ch0 ~/b8_in/b8_ch0.wav --ch1 ~/b8_in/b8_ch1.wav --ch2 ~/b8_in/b8_ch2.wav --ch3 ~/b8_in/b8_ch3.wav \
     --ch4 ~/b8_in/b8_ch4.wav --ch5 ~/b8_in/b8_ch5.wav --ch6 ~/b8_in/b8_ch6.wav --ch7 ~/b8_in/b8_ch7.wav \
     --capture ~/b8_out
python3 $DEMO/pdm_audio_eth/tools/analyze_stream.py ~/b8_out
```

`stream` stops at the end of the longest file. A shorter channel is silent
after its end, unlike the window, which loops. `--capture` writes `in_ch*.wav`
(as sent) and `out_ch*.wav` (from the board). The analysis compares them
channel by channel (expected: -85 to -94 dB residual, no bad 64-frame block).

### 7.7 Board memory checks (optional, CCS memory view)

| Address | What | Expected |
| ------- | ---- | -------- |
| 0x30010000 | ICSSG0 PCM header | MAGIC 0x50444D38, UNDERRUN_COUNT 0, SD_OVF_COUNT 0, HALF_SEQ rising 250/s |
| 0x30090010 | ICSSG1 RX counters | RX_BAD 0, RX_FOREIGN 0 |
| 0x700C0000, 0x40000 bytes | clip buffer | unchanged by `test`: save it and run `python3 $DEMO/pdm_audio_eth/tools/compare_clip_dump.py DUMP` |

## 8. Booth talking points

- **Real time vs simulation:** the same eight-channel PDM chain runs in the
  pru-simulator at about 11 minutes per second of audio. On the board it runs
  in real time, with Gigabit Ethernet in both directions.
- **PRUs only in the data path:** the R5F sets things up and then idles. Five
  PRU cores do everything:
  - RTU1 runs eight sigma-delta modulators;
  - PRU1 drives the PDM clock and data pins;
  - PRU0 decodes eight SD channels (SINC3, FIR, DC blocker) into 16 kHz PCM;
  - ICSSG1 PRU1 receives Ethernet and ICSSG1 PRU0 sends it.
- **Numbers from the board run:**
  - board audio latency 1.08 ms;
  - 600 s capture with no lost frame;
  - 64-byte frames from the board at 98 % of Gigabit line rate;
  - output equals input through the chain to -85 dB or better.
- **Where it goes next:** replace the jumpers and modulators by eight real PDM
  MEMS microphones (PRU1 drives their clock, PRU0 reads them as today). On the
  PC side a GPU stage for speaker separation and noise and echo cancelling
  (the app has the hook, `gui --export-shm`).

## 9. Troubleshooting

| Symptom | Cause, fix |
| ------- | ---------- |
| `pdm-eth: command not found` | not on `PATH`: use `$PDM` or the full path (2.1) |
| `pdm-eth-demo: command not found` | launcher not installed (2.1), or `~/.local/bin` was created in this session: open a new terminal |
| `[pdm-eth-demo] no interface enp129s0` | the NIC has another name: `ip -br link`, then `PDM_IFACE=<name> pdm-eth-demo` |
| `[pdm-eth-demo] warning: no link on ...` | as `link DOWN` below; the window is open and recovers when the link comes up |
| `pdm-eth-demo` stops at a password prompt | a step needs `sudo` (table in 7.1); normal after a rebuild or on a new PC |
| `raw socket on enp129s0 needs CAP_NET_RAW: run sudo setcap ...` | `setcap` missing after a build (2.1); `getcap $PDM` |
| `ethtool`: no link, console `link DOWN` | cable not in J18, board not running, or the NIC is down (`ip link set enp129s0 up`) |
| Board loaded but numbers do not change after a firmware edit | CCS loaded the old image: re-import **and build** (4) |
| PRU1/RTU1 stuck, no audio, BATCH_COUNT frozen | a single core was restarted: run the R5F again (5). Still 0 after that: power-cycle J10 (5) |
| `capture` writes 0 frames, NIC receives 0 frames/s, `test` passes | the ICSSG0 chain is not producing PCM (HALF_SEQ at 0x30010018 stays 0): power-cycle J10 (5) |
| OUT pane empty on every channel, frames arrive, all counters fine, PCM exactly 0 | SD block sees the data inputs stuck at 0 (channel block 0x30000900, 5): power-cycle J10 |
| `from board 64 B` FAIL with `rx_missed` in `ethtool -S` | the PC's NIC (for example RTL8111) cannot take 64-byte frames at line rate; the demo is not affected (6) |
| `make`: `imports.mak: No such file or directory` | new checkout: create `imports.mak` from `imports.mak.default` (4) |
| IN pane still shows the old source after a change | the panes scroll the last 10 s and a change is heard 0.5 s later; check the dropdown label and that the toolbar shows the same channel (7.1, 7.2) |
| `rx_missed` rises in `ethtool -S enp129s0` during `test` | NIC interrupt not on its own CPU (2.3): check the affinity |
| One ping of about 4 ms | PC-side wake-up; harmless (6) |
| No latency line in the side panel | the stream has not started yet (the first second after the start), or a `pdm-eth` from before 2026-11-07 with no source chosen yet: the latency needs a PC stream (7.1) |
| IN pane says "no input stream" | a `pdm-eth` from before 2026-11-07: rebuild (2.1), or choose a source |
| Latency shows "-" or jumps | a pure tone on the selected channel matches at many lags: select a speech, music or noise channel |
| GUI: "board clock" ppm jumps between readings | short-window estimate; known open item, no effect on audio |
| Window does not redraw while a file dialog is open | by design: the dialog blocks the window, the audio keeps running |
| Spectrogram of a channel flat or noise, counters fine | that channel's data jumper (3) |
| Channels 4 to 7 flat, 0 to 3 fine | SW6 not in default (Rev A only), or a BoosterPack plugged in (3) |

## 10. Switching to another demo

PRU1 keeps driving its pins until the SoC is reset, and the i2c_io demo
drives BP.32, which is jumpered to channel 0 here. So before another demo:
**press SW1 (MCU PORZ), pull all nine jumpers**, reconnect CCS, load the
other demo.
