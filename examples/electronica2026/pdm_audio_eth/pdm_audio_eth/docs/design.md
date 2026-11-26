# Design: 8-channel PDM audio over Gigabit Ethernet (electronica 2026)

Date: 2026-11-04 · Status: design, for review · Target: open-pru
`examples/electronica2026/pdm_audio_eth/`, LP-AM243 Rev A, ICSSG0 + ICSSG1,
Ubuntu 26.04 PC

## 1. Intent

The `pdm_audio` demo runs an 8-channel PDM microphone chain on one LP-AM243:
RTU1 modulates 8 clips, PRU1 drives the PDM pins, and PRU0 decodes the pins
to 16 kHz PCM in ICSSG0 shared RAM. It passed its board run on 2026-11-04.
The same chain in the pru-simulator takes about 11 minutes per second of audio
(0.09 s of audio per minute, pru-simulator PDM demo report, 2026-11-03).

This project connects that chain to a PC over Gigabit Ethernet, in both
directions, with PRU firmware only:

- the PC streams long audio files (one mono WAV per channel) into the board;
- the board runs them through the real PDM round trip (modulate, pins, SD
  decode) on all 8 channels;
- the board streams the 8-channel PCM back to the PC;
- the PC shows the input and the output spectrograms side by side, in real
  time.

**The point at the booth:** what takes minutes in the simulator runs live on
the hardware. The visitor sees a spectrogram updating at 30 fps or more next to
numbers that make the speed concrete.

**Next step (not in this project):** post-processing on the PC, starting with
the same power cepstrum as the pru-simulator audio project, to show how fast
it runs on real hardware with the RTX PRO 4000. This design only leaves room
for it.

## 2. Goals, non-goals, anti-goals

**Goals**

1. A basic Ethernet test between the PC and the board: link up, ping-pong
   round-trip latency, throughput in each direction.
2. Board → PC: all 8 channels of 16 kHz PCM, streamed live, with no lost or
   misplaced samples, and with the loss counted if it happens.
3. PC → board: up to 8 mono WAV files, one per channel, streamed into the
   board as the modulator input, for audio much longer than the 0.5 s booth
   clips.
4. A PC application with a live screen at 30 fps or more for at least two
   channels, input next to output (layout C below), and capture of the board
   output to WAV files.

**Non-goals**

- No pru-simulator work: no MII_RT / RGMII model, no harness tests. The board
  is the test bench for the Ethernet part (user decision).
- No change to the `pdm_audio` project. It is booth-ready and stays as it is.
- No IP or UDP stack. The board and PC exchange raw Ethernet frames on a
  direct cable.
- No cepstrum or other post-processing yet (next step), and no speaker
  separation, noise or echo cancellation (a second project on the NVIDIA GPU).
  This project only provides the processing seam (section 7).
- No R5F in the data path (user decision).
- No support for several PCs, switches, Windows or other boards.

**Anti-goals** (a failure even if it technically works)

- Breaking the tested `pdm_audio` demo, or changing its ICSSG0 firmware in a
  way that is not board-tested.
- A stream that loses or shifts audio without saying so: every loss, gap,
  underrun and overflow must show as a counter on the screen.
- A PC application that needs `sudo` on every run, or that stutters while the
  spectrogram runs.

## 3. Constraints

- **Time:** best effort before electronica opens on 2026-11-10. If it works in
  time, it is shown. That is why the work is staged (section 9) and each stage
  is a demo on its own.
- **Hardware:** LP-AM243 Rev A. ICSSG0 is fully used by the PDM chain (RTU1,
  PRU1, PRU0 at 333.33 MHz). Ethernet runs on **ICSSG1, port 1 (RGMII2, PHY
  address 15; the HIL example said 3, corrected after the first board run)**
  as in the user's HIL example. ICSSG1 also runs at 333.33 MHz.
- **PRUs only in the data path.** The R5F (r5fss0-0, FreeRTOS) configures
  both ICSSGs, the pins, the MDIO and the PHY, loads and starts all PRU cores,
  and idles (user decision).
- **Standard Ethernet frames,** at most 1500 bytes of payload. A 64-frame PCM
  half (2048 B) does not fit and is sent as two frames (user).
- **PC:** this PC, Ubuntu 26.04.1 LTS, 20 cores, the on-board Realtek RTL8125
  2.5 GbE port `enp129s0` (driver `r8169`) on a direct cable to the LP. Internet
  stays on WiFi. GPU: NVIDIA RTX PRO 4000 Blackwell, driver 615.71.09, no CUDA
  toolkit installed. Rust and cargo are installed.
- **Data rates:** board → PC 8 ch x 16 kHz x 4 B = 512 KB/s; PC → board
  8 ch x 16 kHz x 2 B = 256 KB/s. Both are small for Gigabit; the design is
  about timing and loss, not bandwidth.

## 4. Reference: the user's HIL example

`HIL_am243x-lp_r5fss0-0_freertos_ti-arm-clang.zip` (user, 2026-11-04) holds
four CCS projects: the R5F application, ICSSG1 PRU0 and PRU1 firmware, and
`pru_eth_init.c` with a register-level write-up
(`pru_eth_init_register_doc.md`). What it shows and this design reuses:

- **R5F init (`PRU_ETH_init`):** pad config of the RGMII2 and MDIO pins, MII_RT
  TX / RX config (TXCFG1 = PRU0 as TX source on port 1, RXCFG1 = port 1 RX with
  RX_EOF on R31 bit 20), G_ICSS_G_CFG (TX_L1/TX_L2/RX_L2 on, RGMII mode),
  G_RGMII_CFG (1000 Mbit/s full duplex), IEP at 333 MHz with an increment of
  3 (the IEP counts in ns), MDIO and the extended PHY registers (RGMII TX delay)
  of both DP83869 PHYs (port 0 address 3, port 1 address 15; swapped in the HIL example).
  `PRU_ETH_wait_for_link(port, timeout)` polls the MDIO link register.
- **ICSSG1 PRU0 (TX):** writes a frame through the TX_L2 FIFO with `xout 40`
  (64 bytes at a time), waits until the FIFO level is 0, then pushes CRC and EOF
  with an R31 command. The frame is a fixed broadcast test frame.
- **ICSSG1 PRU1 (RX):** reads the RX_L2 FIFO in 32-byte banks
  (`RXL2_SIDEA` / `RXL2_SIDEB`, R18 = byte count), checks EOF on R31 bit 20,
  and stores the payload with `sbbo` straight to a **global address in the other
  ICSSG** (ICSSG0 DMEM0 at 0x30000000) while the frame is still arriving. That
  shows a cross-ICSSG write path from ICSSG1. A 32-byte bank fills every
  256 ns, about 85 PRU cycles at 333 MHz (`rx_timing_analysis.html`).

**Status (user, 2026-11-04): Ethernet RX and TX of the HIL example have been
tested on the board.** Stage 1 is therefore a port of working code, not a
fresh bring-up.

As found in the zip (a snapshot that may differ from the tested state): the
call to `PRU_ETH_init()` in `empty_example.c` is commented out, the SysConfig
file has no RGMII / MDIO pin entries, `pru_eth_init.c` writes its pad-unlock
kicks to 0x43001008 / 0x43005008 (not checked against the TRM), and the
SysConfig names the ICSSG1 instance `CONFIG_PRU_ICSS0` and the ICSSG0 instance
`CONFIG_PRU_ICSS1`. The builder ports the init path that brought the link up
in the user's test, and confirms it on the first board run of stage 1.

## 5. Board design

### 5.1 ICSSG0: the PDM chain, unchanged

The new project copies the three `pdm_audio` firmwares (RTU1 `rtu1_mod.asm`,
PRU1 `pru1_pdm.asm`, PRU0 `pru0_rx.asm`) and `pdm8_shared.inc` **unchanged**.
The shared-RAM header and ping-pong layout at 0x30010000 stay as documented in
`pdm_audio/docs/design.md` (HALF_SEQ, PCM_COUNT, halves at +0x100 and +0x900,
UNDERRUN_COUNT, SD_OVF_COUNT, RTU1_FRAME_POS, RTU1_LOOPS).

**My call: no RTU1 change for the PC stream.** RTU1 already loops CLIP_FRAMES
frames from CLIP_ADDR, both read from the header, and publishes the frame it is
modulating (RTU1_FRAME_POS) and the number of loops (RTU1_LOOPS). So the "ring"
is the clip buffer itself:

- the R5F places a larger clip buffer in MSRAM (1 s = 16 000 frames x 16 B =
  256 KB is my starting value) and fills it with the booth clips repeated, so
  the board plays the clips when no PC stream runs (stage 2 needs nothing
  from the PC);
- in stage 3, ICSSG1 PRU1 writes the PC's frames into this buffer ahead of
  RTU1's position, at `sample_index mod CLIP_FRAMES`;
- RTU1's read position is `RTU1_LOOPS x CLIP_FRAMES + RTU1_FRAME_POS`. The
  board reports it to the PC, which keeps its write position a set distance
  ahead (pacing, section 6.4). RTU1_LOOPS and RTU1_FRAME_POS are two words;
  a reader across the interconnect can see them torn at a clip wrap, so the
  reader must allow for that (for example, read twice until both agree).

The cost: if the PC falls behind, RTU1 replays whatever is in the buffer
instead of silence. The PC detects that from the reported positions and counts
it as an input underrun. If that proves a problem on the booth, a later change
can make ICSSG1 PRU1 zero frames RTU1 has passed. That is not part of this
design.

This keeps the board-tested ICSSG0 firmware byte-identical, as the anti-goals
ask. The `pdm_audio` R5F application is copied and extended, not shared.

### 5.2 ICSSG1 PRU0: transmit to the PC

- **Audio out:** PRU0 polls HALF_SEQ in ICSSG0 shared RAM (global address
  0x30010018). When a half is ready (`(HALF_SEQ - 1) & 1`), it sends it as **two
  frames of 32 PCM frames each** (32 x 8 ch x int32 = 1024 B of payload), 500
  frames per second in total. It has 4 ms per half before PRU0 on ICSSG0 starts
  overwriting it.
- **Feeding the FIFO at wire rate:** the TX_L2 FIFO must be kept fed at 8 ns
  per byte. Reads across the interconnect from ICSSG0 shared RAM are much slower
  than that, so PRU0 first copies the 1 KB block into ICSSG1-local memory and
  then sends from there. How (LBBO bursts, the XFR2VBUS DMA) is the builder's
  choice.
- **Ping replies and status:** PRU0 also sends the replies to ping frames and
  the throughput-test frames (section 6). It interleaves them with the audio
  frames; audio has priority.
- **Timestamps:** PRU0 reads the ICSSG1 IEP counter (ns) when it starts each
  frame and puts it in the frame.

### 5.3 ICSSG1 PRU1: receive from the PC

- Reads RX_L2 as in the HIL example and drops every frame that is not this
  project's EtherType (the PC's own traffic: IPv6, LLDP and so on).
- **Audio in:** each PC audio frame carries 64 frames x 8 ch x int16 (1024 B)
  and its target sample index. PRU1 writes the 1024 B to the MSRAM clip buffer
  at `(sample_index mod CLIP_FRAMES) x 16`. Whether it stores straight to MSRAM
  while the frame arrives (as the HIL example does to ICSSG0 DMEM) or first
  buffers locally and copies after EOF is the builder's choice; the board run
  decides.
- **Ping:** PRU1 timestamps the ping on arrival (IEP) and hands it to PRU0 for
  the reply. The PRU0 and PRU1 of ICSSG1 can share scratchpad banks 10 to 12 on
  silicon, or use ICSSG1 shared RAM. That is the builder's choice.
- **Throughput test:** PRU1 counts frames, bytes and errors and publishes the
  counts.
- PRU1 keeps its counters in ICSSG1 shared RAM (0x30090000), where PRU0 can
  copy them into outgoing frames and CCS can read them.

### 5.4 ICSSG1 other cores, R5F, memory

- ICSSG1 RTU0/RTU1/TX_PRU0/TX_PRU1 are not used; the R5F halts them.
- The R5F does everything in `pdm_audio` (ICSSG0) plus the ICSSG1 and PHY
  setup from the HIL example, then starts ICSSG1 PRU1, ICSSG1 PRU0, then the
  ICSSG0 cores in the `pdm_audio` order. The exact start order across the two
  ICSSGs is the builder's choice.
- SysConfig: one `example.syscfg` with both ICSSG instances at 333.33 MHz, the
  `pdm_audio` pins, and the RGMII2 / MDIO pins. Instance names should match the
  hardware this time (ICSSG0 = `CONFIG_PRU_ICSS0`, ICSSG1 = `CONFIG_PRU_ICSS1`),
  my call.

## 6. Ethernet protocol

Raw Ethernet II frames, direct cable, **my calls:**

- **EtherType 0x88B5** (IEEE 802 local experimental).
- **Board MAC:** a fixed locally administered address (for example
  02:00:5D:4D:00:01). The board replies to the source MAC of the last valid PC
  frame, so the PC needs no configuration.
- **Common header** after the EtherType: magic and version, frame type, a
  16- or 32-bit sequence number per type, the board's IEP timestamp (board
  frames), and a payload length. The byte layout is the builder's, documented
  in one shared `.inc` / `.h` / Rust module that tests check against each other
  (as `pdm8_shared.inc` and `pdm8_shared.h` are checked today).

### 6.1 Frame types

| Type | Direction | Payload |
|------|-----------|---------|
| PING | PC → board | PC send time, sequence, padding to a chosen size |
| PONG | board → PC | the PING's sequence and PC time, the board's RX and TX IEP times |
| TPUT_DATA | both | sequence and filler, maximum size |
| TPUT_REPORT | board → PC | board RX counts (frames, bytes, sequence gaps, errors) |
| AUDIO_IN | PC → board | target sample index, 64 frames x 8 ch x int16 (1024 B) |
| AUDIO_OUT | board → PC | first PCM sample index, IEP time, status block, 32 frames x 8 ch x int32 (1024 B) |
| CONTROL | PC → board | start/stop throughput test, reset counters (builder's set) |

**The AUDIO_OUT status block** carries what the PC needs to pace and check:
HALF_SEQ, RTU1 read position (`RTU1_LOOPS`, `RTU1_FRAME_POS`), CLIP_FRAMES,
UNDERRUN_COUNT, SD_OVF_COUNT, the ICSSG1 RX counters, and the last AUDIO_IN
sample index PRU1 stored.

### 6.2 Reconstruction (user)

The first PCM sample index of an AUDIO_OUT frame is `HALF_SEQ x 64` for the
first half of a half and `HALF_SEQ x 64 + 32` for the second. The PC places
each frame by this index. A missing index is a lost frame, counted and shown
(the gap is filled with silence in the display and the capture). The IEP time
lets the PC compare the board's sample clock with its own and show the send
jitter.

### 6.3 Ethernet test

1. **Link:** the R5F waits for link on port 1; the PC checks `enp129s0` is up
   at 1000 Mbit/s full duplex (2.5 GbE card, 1 GbE link).
2. **Ping-pong:** N PINGs (for example 10 000), one outstanding at a time. The
   PC reports round-trip min / mean / p99 / max and the spread, and the board's
   own turnaround from the PONG's IEP times (RX start to TX start).
3. **Throughput:** each direction for a few seconds at maximum frame size,
   back to back. The receiver reports frames, bytes, Mbit/s, sequence gaps.
   PC → board is counted by PRU1 (TPUT_REPORT); board → PC by the PC. A loss on
   the PC side must be reported as such, not blamed on the board.
4. **Summary:** one pass/fail line per step. **My starting thresholds:** link
   1000/full; ping 0 lost; throughput each way at least 500 Mbit/s with less than
   0.1 % loss. The real numbers from the first board run decide the final
   thresholds.

### 6.4 Pacing PC → board

The PC sends AUDIO_IN frames (250 per second for 16 kHz) and keeps its next
sample index a target distance ahead of the board's reported RTU1 read
position (my starting value: 0.5 s of the 1 s buffer). Too close means an input
underrun, too far would overwrite audio RTU1 has not read yet: both are
counted. The PC never sends faster than the board consumes plus a small
correction.

## 7. PC application

**Language and libraries, my calls:** one Rust program, native window with
egui / eframe on wgpu (GPU drawing). Raw Ethernet through a Linux `AF_PACKET`
socket on `enp129s0`, ideally with the memory-mapped ring (`TPACKET_V3`). The
binary gets `cap_net_raw` once with `setcap`, so it runs without `sudo`.
`enp129s0` is set to "unmanaged" (or no IP) in NetworkManager so Ubuntu does not
send its own traffic on the cable; the board drops anything else anyway.

**Location:** `examples/electronica2026/pdm_audio_eth/pc_app/` (Rust crate),
next to the board project `examples/electronica2026/pdm_audio_eth/pdm_audio_eth/`,
like `i2c_io_expander/visualizer` (user: new project under electronica2026;
names are my call).

**Why Rust, given the GPU projects to come.** This project needs low-latency
raw Ethernet, a steady 30+ fps GPU-drawn screen and no garbage-collector
pauses, which Rust gives with one binary and no runtime. The next projects
use the NVIDIA GPU: first the power cepstrum, then (a second project, user)
a complex audio pipeline with speaker separation, noise cancellation and echo
cancellation. Those are best served by the NVIDIA stack (CUDA and cuFFT for
DSP; PyTorch, ONNX Runtime with the CUDA / TensorRT providers, or NVIDIA's
audio SDKs for the models; to be evaluated in that project). So the PC
application gets one **processing seam** now, with no processing behind it
yet:

- every channel's input and output stream is available as aligned 16 kHz
  blocks with sample indices, to an in-process processing stage (a Rust trait);
- the same blocks can be exported to a separate process through shared memory
  or a local socket, so a Python or CUDA C++ pipeline can run beside the Rust
  application without being linked into it;
- the screen can show the result of a stage as another spectrogram and its
  processing time per block.

Rust can call CUDA directly (for example the `cudarc` crate for cuFFT and
kernels, or ONNX Runtime), so the cepstrum step can stay in-process; the
speaker-separation project may prefer the separate-process route. That choice
is made in those projects. The CUDA toolkit is not installed on the PC yet.

### 7.1 Functions

- **Eth test:** section 6.3, results on screen and as a text summary on the
  command line.
- **Stream:** pick up to 8 WAV files and assign each to a channel. Each file is
  converted to 16 kHz mono int16 on load (resampled; stereo mixed down) and
  scaled to a peak of 0.5 FS like the booth clips, because input near full
  scale wraps the RTU1 modulators (`pdm_audio` readme); my call.
  A channel without a file, or after its file ends, plays silence. The stream
  ends when the longest file ends.
- **Capture:** saves the board output as one mono WAV per channel, 16 kHz,
  24-bit (the PCM is 24-bit saturated in int32; my call), plus the input as sent.
- **Live screen** (7.2).
- Without a PC stream, the board plays the booth clips, and the screen shows
  their output (stage 2).

### 7.2 Screen: layout C (user)

The mockup is `docs/screen_mockup.html`, option C (A and B were the
alternatives).

- **Main area:** one channel large: the PC input spectrogram on top, the board
  output spectrogram below, and a thin difference strip (input minus output,
  what the PDM round trip changed). **My call:** the strip is the per-bin dB
  difference of the two spectrograms, aligned by sample index, not a
  sample-level residual (that would need exact delay, gain and phase matching
  through the FIR, the DC blocker and the odd-channel stagger).
- **Channel selector.** The PC computes the spectrograms of all 8 channels all
  the time, so switching is instant and keeps history; at least two channels
  must run live at 30 fps or more (user). A second channel shown at the same
  time is the builder's option.
- **Side panel of live numbers:** board audio latency (ms, see below), PRU
  ping turnaround, frames per second each way, spectrogram fps, lost frames /
  input underruns / board UNDERRUN_COUNT / SD_OVF_COUNT, and the simulator
  comparison ("about 11 min of simulation per second of audio", a fixed
  number from the pru-simulator report).
- **Toolbar:** Stream, Capture, Eth test, channel selector, link state.
- **Spectrogram:** the same as the pru-simulator Audio view (256-point Hann,
  hop 128 = 8 ms per column, -90 to 0 dB, 0 to 8 kHz), so the two can be put side
  by side.
- **Room for later:** the cepstrum step will add a processing view and a
  "processing time per frame" number; the layout leaves space for them.
- **Board audio latency (user: option A).** The number on the screen is the
  board's own delay: from the time RTU1 takes an input sample out of the clip
  buffer to the time the PCM of that sample leaves the board in an AUDIO_OUT
  frame. It is measured from the sample indices and the reported positions
  (RTU1 read position, PCM sample index, IEP time), not a constant. It
  excludes the PC's 0.5 s pacing lead, which stays as designed (section 6.4);
  the lead can be shown as a separate number ("buffered ahead") so the screen
  does not hide it.

### 7.2a Custom audio per channel in the GUI (added 2026-11-05, user)

After the board run the user asked for custom audio per channel in the
window and for example audio with speech and music (7.2b).

- **First try, removed: drag and drop.** Built and unit-tested, but on the
  user's Ubuntu Wayland session a drop did nothing: winit 0.30.13 (under
  eframe) delivers file-drop events only on Windows, macOS and X11, its
  Wayland backend has no drag-and-drop. The user then chose a selector per
  channel instead.
- **Source selector per channel (user: all 8 visible):** a "Sources" table
  at the top of the side panel, one dropdown per channel: *booth clip* (the
  channel's clip of `mcuplus/pdm_clips.h`), *speech*, *music*, *speech +
  music* (the files of 7.2b, compiled in), *silence*, and *open WAV file...*
  (the desktop's file dialog through the XDG portal, `rfd`, works on
  Wayland). A file is converted as `--chN` (16 kHz mono, peak 0.5 FS); one
  that does not load shows its error in the toolbar and changes nothing.
- **Live:** the UI hands the samples to the network thread
  (`NetCmd::SetChannel`). Without a running stream, the first choice starts
  one (the pacer starts one lead ahead, as at start-up); later choices
  replace one channel in place, the stream keeps running.
- **The GUI streams from the start (2026-11-07, user):** `gui` without
  `--chN` files starts with every channel on its booth clip
  (`InputSet::gui_start`), so the IN pane shows what the board plays from the
  first second. It also rewrites the board's clip buffer with the real booth
  clips: the clip buffer is the input ring (5.1), so a closed GUI leaves its
  last sources in it, and a GUI that did not stream showed the previous
  session's leftovers (for example *silence*) as the "booth clips". Before,
  `gui` without files streamed nothing until the first choice.
- **Channels nobody chose play their booth clip:** the PC sends that
  channel's clip, so changing one channel does not silence the other seven.
  This also applies to `gui --chN` started with fewer than 8 files (before:
  silence). `stream` keeps its silence rule (7.1), which the B8 analysis
  relies on.
- **Looping per channel:** in the GUI each channel loops on its own length
  (a 30 s music file next to the 1.024 s clips). Before, all channels
  looped on the longest file and the shorter ones were silent until then.
- **Labels:** the IN pane names the channel's source.
- **A new source starts at its beginning:** each channel counts from the
  sample at which its source was set (before: one shared counter, so a
  30 s file chosen 40 s into the session started 10 s in).
- **Capture sets (user):** for real multi-microphone captures, which must
  stay sample-aligned. "open capture set..." above the table multi-selects
  up to 8 mono WAV files (user: mono files, not one multichannel file);
  sorted by file name they go to ch0, ch1, ... (channels beyond the set
  keep their sources). The set is loaded as a whole:
  - all files mono and at one sample rate, else the set is refused with
    an error naming the file;
  - trimmed to the shortest file from a common start (user), so every
    channel has the same length and they loop together; the side panel
    shows the length and how many samples were cut;
  - **one gain for the set:** its loudest sample maps to 0.5 FS, so the
    level differences between the microphones are kept (a single file is
    still scaled on its own);
  - handed to the network thread in one command (`NetCmd::SetSet`), so all
    its channels start on the same sample.

### 7.2b Example audio

`pc_app/audio/`: about 30 s each of speech (LibriVox, the recording of the
booth speech clip), music (a public-domain recording) and both mixed, as
16 kHz mono WAV, with their origin and licence in `pc_app/audio/SOURCES.md`.

### 7.3 PC-side testing

The board is the Ethernet test bench, but the PC application should not need
the board for every change. **My call:** a small board emulator in the crate
(same frame formats, on a `veth` pair or in-process) for UI and protocol work,
and unit tests for frame encode/decode, reconstruction, gap handling,
resampling and WAV writing.

## 8. Verification on the board

As for `pdm_audio`: CCS memory browser plus the PC application.

- Ethernet test results (section 6.3) recorded in the readme.
- Stage 2: a 10-minute run of the board → PC stream with 0 lost frames, the
  PCM matching the ICSSG0 shared RAM (spot check in CCS), tone frequencies and
  levels as in the `pdm_audio` hardware results.
- Stage 3: a long WAV per channel through the loop, input and output
  spectrograms matching, the input underrun and overwrite counters at 0, the
  board audio latency measured.
- The `pdm_audio` hardware checks still pass in the new project (UNDERRUN_COUNT
  and SD_OVF_COUNT 0, PCM_COUNT spread).

## 9. Stages (user: staging is fine)

Each stage is a booth demo on its own; if time runs out, the last finished
stage is shown.

1. **Ethernet test.** ICSSG1 PRU0/PRU1 firmware, R5F ICSSG1/PHY setup, PC
   "Eth test" (link, ping-pong, throughput).
2. **Board → PC audio.** The PDM chain as in `pdm_audio` with the booth clips;
   ICSSG1 PRU0 streams all 8 channels; the PC shows the live output
   spectrograms (layout C without the input) and captures WAVs.
3. **PC → board audio.** The PC streams WAV files into the MSRAM clip buffer
   through ICSSG1 PRU1, paced by the board's read position; layout C shows input
   and output side by side.
4. **Later:** the cepstrum (not in this project).

## 10. Left to the builder

- The exact frame byte layouts, sequence widths and CONTROL commands.
- How PRU0 copies ICSSG0 PCM into ICSSG1-local memory, and how PRU1 writes
  into MSRAM (direct or buffered).
- The ICSSG1 PRU0 ↔ PRU1 handoff for pings.
- The start order across the two ICSSGs.
- PC crate structure, the emulator, and the exact board-latency computation.
- The final pass/fail thresholds of the Ethernet test, from the first board run.

## 11. My calls, for review

- No RTU1 change: the PC stream writes into the clip buffer RTU1 already
  loops (section 5.1).
- A 1 s clip buffer (256 KB MSRAM) pre-filled with the booth clips, and a 0.5 s
  target lead for the PC.
- EtherType 0x88B5, a fixed locally administered board MAC, replies to the
  last PC MAC.
- Rust + egui / eframe / wgpu + AF_PACKET with `setcap`; NetworkManager
  "unmanaged" for `enp129s0`; a processing seam (in-process trait plus
  shared-memory / socket export) for the later GPU projects.
- The difference strip is a spectral dB difference; WAV input scaled to 0.5 FS.
- WAV conversion on load (16 kHz mono int16), silence after a file ends,
  24-bit capture.
- Ethernet test starting thresholds (500 Mbit/s, < 0.1 % loss, 0 lost pings).
- A board emulator for PC development.
- Folder names `pdm_audio_eth/pdm_audio_eth` and `pdm_audio_eth/pc_app`;
  SysConfig instance names that match the hardware.
- The branch `dev_tley_electronica2026_pdm_audio_eth` is stacked on
  `dev_tley_electronica2026_pdm_audio`, because the project starts from copies
  of its files.
