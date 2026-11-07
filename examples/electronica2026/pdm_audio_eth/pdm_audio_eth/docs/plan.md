# PDM audio over Gigabit Ethernet: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stream PC WAV files through the LP-AM243 8-channel PDM chain and the 8-channel PCM back over Gigabit Ethernet on ICSSG1 (PRUs only), with a Rust PC application for the Ethernet test, live input/output spectrograms (layout C) and capture.

**Architecture:** ICSSG0 runs the board-tested `pdm_audio` firmware byte-identical; the clip buffer RTU1 loops (now 16 000 frames in MSRAM) doubles as the PC input ring. ICSSG1 PRU1 receives raw frames (EtherType 0x88B5) and writes PC audio into that buffer; ICSSG1 PRU0 copies each finished PCM half from ICSSG0 shared RAM and sends it as two frames, plus pongs and throughput frames. The R5F only configures, loads and starts. A Rust program talks AF_PACKET on `enp129s0`, reconstructs by sample index, paces its input from the board's RTU1 position, and draws spectrograms with egui/wgpu.

**Tech Stack:** TI PRU assembly (clpru 2.3.3, PRU v4), MCU+ SDK AM243x 12.01.00.33, FreeRTOS on r5fss0-0, SysConfig 1.28.1, CCS 21.0.1; Rust 1.99 / edition 2024, eframe 0.37, libc, hound, rubato, rustfft, clap.

**Spec:** `examples/electronica2026/pdm_audio_eth/pdm_audio_eth/docs/design.md` (approved 2026-11-04). Read it with this plan.

## Global Constraints

- Board: LP-AM243 Rev A. ICSSG0 and ICSSG1 core clocks 333 333 333 Hz. Ethernet on ICSSG1 port 1 (RGMII2, PHY address 3), 1000 Mbit/s full duplex.
- ICSSG0 firmware (`rtu1_mod.asm`, `pru1_pdm.asm`, `pru0_rx.asm`, `pdm8_shared.inc`, `fir_coefs.inc`, `decimation_fir.h`) stays byte-identical to `pdm_audio` (checked by `tools/check_icssg0_copy.sh`).
- PRUs only in the data path; the R5F configures, loads, starts and idles.
- No pru-simulator changes; Ethernet is verified on the board.
- `examples/electronica2026/pdm_audio/` is not modified.
- Frames: Ethernet II, EtherType **0x88B5**, payload ≤ 1500 B, all fields after the EtherType little-endian.
- Board MAC **02:00:5D:4D:00:01**; the board replies to the source MAC of the last valid frame.
- Clip buffer: **16 000 frames x 16 B = 256 000 B** at MSRAM **0x700C0000**, region size **0x40000**, pre-filled with the booth clips (repeated).
- PC lead target **8000 frames (0.5 s)**; AUDIO_IN carries **64 frames** (1024 B), AUDIO_OUT carries **32 frames** (1024 B).
- Spectrogram: 256-point Hann, hop 128, −90 to 0 dB, 0 to 8 kHz, ≥ 30 fps for at least two channels.
- PC: Ubuntu 26.04, interface `enp129s0`, binary gets `cap_net_raw,cap_net_admin+ep` via `setcap`; no `sudo` at run time.
- Ethernet test starting thresholds: link 1000/full, 0 lost pings of 10 000, ≥ 500 Mbit/s each way with < 0.1 % loss.
- Builds: from the project directory only, `make DEVICE=am243x MCU_PLUS_SDK_PATH=$HOME/ti/mcu_plus_sdk_am243x_12_01_00_33 SYSCFG_NODE=$HOME/ti/ccs2101/ccs/tools/node/node`; never from the open-pru root.
- Commits in open-pru: `git -c user.name="Thomas Leyrer" -c user.email=t-leyrer@ti.com commit`; never stage `examples/electronica2026/{i2c_io_expander,uart_print,uart_rx_frame}/` (untracked copies from other branches).
- PRU asm: `clpru -v4` with 0 diagnostics, and the pru-assembly linter `python3 ~/.claude/skills/pru-assembly/scripts/lint_pru.py --openpru <file>` with 0 errors.

## Review Focus

1. **The PC's own traffic on the cable** (IPv6 RA/ND, LLDP, mDNS from Ubuntu before NetworkManager is set to unmanaged): the board must drop it and count it, never treat it as audio or a ping — test in Task 4 (frame filter) and Task 7 (`emulator_ignores_foreign_ethertype`).
2. **Board reset or reload while the PC application runs** (HALF_SEQ and sample indices jump back to 0): the PC must resynchronise instead of counting millions of lost frames or stalling — `rx_resync_on_index_jump_back` in Task 9, `pacing_restarts_after_board_reset` in Task 13.
3. **WAV files that are not 16 kHz mono 16-bit** (44.1/48 kHz, stereo, 24-bit, float, 8-bit, very short, empty, missing): load must convert or fail with a clear message per file — `wav_*` tests in Task 13.
4. **The PC application falling behind** (window dragged, laptop busy): the input lead drains; it must count input underruns and recover the lead, not overwrite unread audio — `pacing_counts_underrun_and_recovers`, `pacing_never_writes_past_read_position` in Task 13.
5. **Throughput-test loss on the PC side** (RTL8125 / socket buffer drops at ~81 000 frames/s): it must be reported as PC-side loss with socket drop counters, not as a board failure — `tput_report_splits_pc_and_board_loss` in Task 8.

---

## File Structure

```
examples/electronica2026/pdm_audio_eth/
  pdm_audio_eth/                       board project (copied from pdm_audio, then extended)
    makefile                           PROJECT_NAME pdm_audio_eth; 5 PRU fw + R5F
    readme.md                          Task 16
    docs/design.md, docs/plan.md, docs/screen_mockup.html
    clips/                             copied unchanged
    tools/make_clip_buffer.py          copied; gains --frames
    tools/gen_fir_coefs.py             copied unchanged
    tools/check_icssg0_copy.sh         Task 1: cmp against ../../pdm_audio/pdm_audio
    firmware/
      rtu1_mod.asm pru1_pdm.asm pru0_rx.asm pdm8_shared.inc fir_coefs.inc decimation_fir.h   byte-identical copies
      eth_proto.inc                    Task 2: frame layout + ICSSG1 shared-RAM map
      eth_rx.asm                       ICSSG1 PRU1 (Tasks 4, 12)
      eth_tx.asm                       ICSSG1 PRU0 (Tasks 5, 10)
      am243x-lp/icss_g0_{rtu_pru1,pru1,pru0}_fw/ti-pru-cgt/   copied, names swapped
      am243x-lp/icss_g1_{pru0,pru1}_fw/ti-pru-cgt/            new (Task 3)
    mcuplus/
      pdm8_shared.h                    copied unchanged
      eth_proto.h                      Task 2: C mirror of eth_proto.inc
      pdm_clips.h                      regenerated, 16 000 frames
      eth_init.c / eth_init.h          Task 3: ported from HIL pru_eth_init.c
      pdm_audio_eth_example.c          Task 3: pdm_audio setup + ICSSG1
      am243x-lp/r5fss0-0_freertos/{example.syscfg, main.c, ti-arm-clang/*}
  pc_app/                              Rust crate pdm_eth, binary pdm-eth
    Cargo.toml
    src/lib.rs        module list
    src/proto.rs      Task 2: constants, encode/decode
    src/link.rs       Task 6: Link trait, AfPacketLink
    src/emulator.rs   Task 7: in-process board emulator (Link)
    src/ethtest.rs    Task 8: link / ping / throughput / summary
    src/rx.rs         Task 9: AUDIO_OUT reconstruction, stats, capture
    src/spectro.rs    Task 11: spectrogram columns, difference strip
    src/ui.rs         Tasks 11, 14: layout C
    src/wavin.rs      Task 13: WAV load/convert
    src/pacing.rs     Task 13: AUDIO_IN scheduling against RTU1 position
    src/latency.rs    Task 14: board audio latency
    src/seam.rs       Task 15: processing seam
    src/main.rs       CLI: test | capture | stream | gui
    tests/proto_mirror.rs   Task 2: .inc/.h/.rs agree
```

Rust unit tests live next to the code (`#[cfg(test)]`); run all with `cargo test` in `pc_app/`.

## Protocol (pinned here; Task 2 writes it into all three mirrors)

Ethernet header: dst(6) src(6) EtherType 0x88B5 (big-endian on the wire, as every EtherType).
Common header, 16 B at payload offset 0:

| Off | Type | Field |
|----:|------|-------|
| 0 | u8 | `version` = 1 |
| 1 | u8 | `type` |
| 2 | u16 | `length` = payload bytes after this header |
| 4 | u32 | `seq` (per type, per sender) |
| 8 | u64 | `ts` IEP ns at TX start (board frames); PC monotonic ns (PC frames) |

Types: `PING=1 PONG=2 TPUT_DATA=3 TPUT_REPORT=4 AUDIO_IN=5 AUDIO_OUT=6 CONTROL=7`.

| Type | Body after the common header |
|------|------------------------------|
| PING | u64 `pc_ns`; zero padding to the requested size |
| PONG | u32 `ping_seq`, u32 0, u64 `pc_ns` (echo), u64 `rx_iep` (PRU1 at RX), header `ts` = TX IEP |
| TPUT_DATA | filler up to `length` |
| TPUT_REPORT | u32 `frames`, u32 `seq_gaps`, u64 `bytes`, u64 `first_iep`, u64 `last_iep` |
| AUDIO_IN | u32 `first_index` (multiple of 64), u32 0, 64 x 8 x i16 frame-interleaved |
| AUDIO_OUT | 64-byte status block, then 32 x 8 x i32 frame-interleaved |
| CONTROL | u32 `cmd`, u32 `arg0`, u32 `arg1` |

CONTROL `cmd`: `1 RESET_COUNTERS`, `2 TPUT_TX(arg0 = frame count, arg1 = payload length)`, `3 TPUT_REPORT_REQ`.

AUDIO_OUT status block (u32 unless noted), offsets from the block start:

| Off | Field | Source |
|----:|-------|--------|
| 0 | `first_index` = HALF_SEQ_done x 64 + part x 32 | PRU0/ICSSG1 |
| 4 | `half_seq` | ICSSG0 hdr 0x18 |
| 8 | `rtu1_loops` | ICSSG0 hdr 0x28 (read twice until loops and pos agree) |
| 12 | `rtu1_frame_pos` | ICSSG0 hdr 0x24 |
| 16 | `clip_frames` | ICSSG0 hdr 0x14 |
| 20 | `underrun_count` | ICSSG0 hdr 0x1C |
| 24 | `sd_ovf_count` | ICSSG0 hdr 0x30 |
| 28 | `batch_count` | ICSSG0 hdr 0x20 |
| 32 | `rx_frames` | ICSSG1 shared |
| 36 | `rx_audio` | ICSSG1 shared |
| 40 | `rx_foreign` (other EtherType/MAC) | ICSSG1 shared |
| 44 | `rx_bad` (bad version/type/length) | ICSSG1 shared |
| 48 | `last_in_end` = first_index + 64 of the last stored AUDIO_IN | ICSSG1 shared |
| 52 | `tx_late` (halves seen more than one half late) | ICSSG1 PRU0 |
| 56 | u64 `detect_iep` (IEP when the new HALF_SEQ was seen) | ICSSG1 PRU0 |

`HALF_SEQ_done` is the HALF_SEQ value that announced the half; the half's buffer is `(HALF_SEQ_done − 1) & 1` (A = 0x30010100, B = 0x30010900), its first PCM index is `(HALF_SEQ_done − 1) x 64`. So `first_index = (HALF_SEQ_done − 1) x 64 + part x 32`.

ICSSG1 shared RAM map (local 0x10000, global 0x30090000), `ETH1_*` in `eth_proto.inc`:

| Off | Field | Writer |
|----:|-------|--------|
| 0x00 | MAGIC 0x31485445 ('ETH1') | R5F |
| 0x04 | VERSION 1 | R5F |
| 0x08 | PC_MAC bytes 0-3 | PRU1 |
| 0x0C | PC_MAC bytes 4-5, u16 `mac_valid` | PRU1 |
| 0x10 | RX_FRAMES | PRU1 |
| 0x14 | RX_AUDIO | PRU1 |
| 0x18 | RX_FOREIGN | PRU1 |
| 0x1C | RX_BAD | PRU1 |
| 0x20 | LAST_IN_END | PRU1 |
| 0x24 | PING_REQ (u32 seq + 1, 0 = none) | PRU1 sets, PRU0 clears |
| 0x28 | PING_PC_NS (u64) | PRU1 |
| 0x30 | PING_RX_IEP (u64) | PRU1 |
| 0x38 | TPUT_TX_COUNT (0 = idle) | PRU1 sets, PRU0 clears |
| 0x3C | TPUT_TX_LEN | PRU1 |
| 0x40 | TPUT_RX_FRAMES | PRU1 |
| 0x44 | TPUT_RX_GAPS | PRU1 |
| 0x48 | TPUT_RX_BYTES (u64) | PRU1 |
| 0x50 | TPUT_RX_FIRST_IEP (u64) | PRU1 |
| 0x58 | TPUT_RX_LAST_IEP (u64) | PRU1 |
| 0x60 | REPORT_REQ (0/1) | PRU1 sets, PRU0 clears |
| 0x64 | TX_AUDIO | PRU0 |
| 0x68 | TX_PONGS | PRU0 |
| 0x6C | TX_LATE | PRU0 |
| 0x70 | CLIP_ADDR copy (global) | R5F |
| 0x74 | CLIP_FRAMES copy | R5F |

Fixed hardware values (from the HIL example): TX_L2 XFR ID 40; RX_L2 banks `RXL2_SIDEA` 20 / `RXL2_SIDEB` 21 (R18.b0 = byte count, 65-byte XIN to R2..R18); RX EOF on R31 bit 20; RX reset/EOF clear `or r31.b2, r31.b2, 0xF4`; TX FIFO reset `ldi r31.b3, 0x40`; TX end `ldi r31.w2, 0x0800 | 0x0400 | 0x2000` (CRC MS/LS word + EOF); IEP0 at local 0x0002E000, COUNT low 0x0C, high 0x10, counts ns (CFG 0x331, increment 3).

---

## Stage 1: Ethernet test

### Task 1: Project copy, bigger clip buffer, ICSSG0 unchanged

**Files:**
- Create: everything under `pdm_audio_eth/pdm_audio_eth/` copied from `pdm_audio/pdm_audio/` (except `docs/`); `tools/check_icssg0_copy.sh`
- Modify (in the copy): `makefile` (`PROJECT_NAME := pdm_audio_eth`), every projectspec / makefile name `pdm_audio` → `pdm_audio_eth` and every path `pdm_audio/pdm_audio` → `pdm_audio_eth/pdm_audio_eth`; `tools/make_clip_buffer.py` (`--frames`, default 8000); `mcuplus/pdm_clips.h` (regenerated); `example.syscfg` (`PDM_CLIPS` size 0x40000, `CONFIG_MPU_REGION5.size = 18`); `mcuplus/pdm_audio_example.c` → renamed `pdm_audio_eth_example.c`, entry `pdm_audio_eth_example_main`, `main.c` updated.

**Interfaces:**
- Produces: `make` target that builds the ICSSG0 trio + R5F as `pdm_audio_eth`; `PDM_CLIP_FRAMES == 16000`; `tools/check_icssg0_copy.sh` exit 0 iff the six ICSSG0 firmware files are byte-identical to `../../pdm_audio/pdm_audio/firmware/`.

- [ ] **Step 1:** Copy the tree (`cp -a`, then `rm -rf` any build output and `docs/`), rename as listed. `grep -rn "pdm_audio/pdm_audio\|PROJECT_NAME := pdm_audio$" pdm_audio_eth/` must show only intended hits (none outside docs).
- [ ] **Step 2:** Write `tools/check_icssg0_copy.sh` (`cmp` each of `rtu1_mod.asm pru1_pdm.asm pru0_rx.asm pdm8_shared.inc fir_coefs.inc decimation_fir.h`, and `mcuplus/pdm8_shared.h`); run it: exit 0.
- [ ] **Step 3:** Add `--frames N` to `make_clip_buffer.py` (shorter clips repeat from their first frame, as today); `--check` uses the same N. Regenerate: `python3 tools/make_clip_buffer.py --frames 16000`; header shows `#define PDM_CLIP_FRAMES 16000`, array `16000*8`.
- [ ] **Step 4:** SysConfig: region `PDM_CLIPS` size `0x40000`; MPU region 5 size `18` (256 KB, base 0x700C0000 is 256 KB-aligned); update its comment (256 KB, ends 0x700FFFFF, clear of the SBL at 0x70180000).
- [ ] **Step 5:** Build with the Global Constraints make line from `pdm_audio_eth/pdm_audio_eth`. Expected: exit 0, only the inherited SysConfig part-number warning, `pdm_audio_eth.release.appimage.hs_fs` exists; the `.map` file places `.pdm_clips` at 0x700C0000 with size 0x3E800.
- [ ] **Step 6: Board check (user).** Load and run the R5F in CCS. Memory browser at 0x30010000: CLIP_FRAMES = 0x3E80, UNDERRUN_COUNT 0, SD_OVF_COUNT 0, HALF_SEQ rising, tone channels 3–7 as in the `pdm_audio` hardware results.
- [ ] **Step 7: Commit** `feat(electronica2026): pdm_audio_eth project from pdm_audio, 1 s clip buffer`.

### Task 2: Protocol definitions in three mirrors

**Files:**
- Create: `firmware/eth_proto.inc`, `mcuplus/eth_proto.h`, `pc_app/Cargo.toml`, `pc_app/src/lib.rs`, `pc_app/src/proto.rs`, `pc_app/tests/proto_mirror.rs`, `pc_app/src/main.rs` (CLI stub: `pdm-eth --help`)

**Interfaces:**
- Produces (`proto.rs`): `pub const ETHERTYPE: u16 = 0x88B5; pub const BOARD_MAC: [u8;6]`; `#[repr(u8)] pub enum FrameType {Ping=1,..,Control=7}`; `pub struct Header {version:u8, ftype:FrameType, length:u16, seq:u32, ts:u64}`; `pub struct Status {first_index:u32, half_seq:u32, rtu1_loops:u32, rtu1_frame_pos:u32, clip_frames:u32, underrun_count:u32, sd_ovf_count:u32, batch_count:u32, rx_frames:u32, rx_audio:u32, rx_foreign:u32, rx_bad:u32, last_in_end:u32, tx_late:u32, detect_iep:u64}` with `fn rtu1_total(&self) -> u64` (= loops x clip_frames + pos); `pub enum Body {Ping{pc_ns:u64}, Pong{ping_seq:u32,pc_ns:u64,rx_iep:u64}, TputData, TputReport{frames:u32,seq_gaps:u32,bytes:u64,first_iep:u64,last_iep:u64}, AudioIn{first_index:u32, pcm:Box<[[i16;8];64]>}, AudioOut{status:Status, pcm:Box<[[i32;8];32]>}, Control{cmd:u32,arg0:u32,arg1:u32}}`; `pub fn encode(dst:[u8;6], src:[u8;6], h:&Header, b:&Body, pad_to:usize) -> Vec<u8>`; `pub fn decode(frame:&[u8]) -> Result<(Header, Body, [u8;6] /*src*/), DecodeError>`; `DecodeError::{Short, ForeignEtherType, BadVersion, BadType, BadLength}`.
- `.inc`/`.h` names: `ETH_` + field names in upper case, e.g. `ETH_HDR_SEQ .set 4`, `ETH_ST_DETECT_IEP .set 56`, `ETH1_PING_REQ .set 0x24`; values exactly as in "Protocol" above.

- [ ] **Step 1: Failing tests** in `proto.rs`: `roundtrip_every_type` (encode → decode equality for all seven bodies), `audio_out_frame_is_1118_bytes` (`14 + 16 + 64 + 1024`), `audio_in_frame_is_1062_bytes` (`14 + 16 + 8 + 1024`), `decode_rejects_foreign_ethertype` (0x86DD → `ForeignEtherType`), `decode_rejects_short_and_bad_length`, `ethertype_is_big_endian_fields_little_endian` (bytes 12..14 = `88 B5`, `seq` 0x01020304 at payload+4 = `04 03 02 01`), `status_rtu1_total` (loops 3, pos 5, frames 16000 → 48005).
- [ ] **Step 2:** `cargo test` → fail (not defined).
- [ ] **Step 3:** Implement `proto.rs`; write `eth_proto.inc` (plain-literal `.set`, as `pdm8_shared.inc`) and `eth_proto.h` (`#define`).
- [ ] **Step 4: Mirror test** `tests/proto_mirror.rs::inc_h_rs_agree`: parse every `ETH_*`/`ETH1_*` name from the `.inc` (`NAME .set VALUE`) and the `.h` (`#define NAME VALUE`), assert both sets equal and each value equals the matching Rust constant in `proto::mirror::ALL: &[(&str, u64)]`.
- [ ] **Step 5:** `cargo test` → all pass; `clpru -v4` assembles a 2-line file that `.include`s `eth_proto.inc` with 0 diagnostics.
- [ ] **Step 6: Commit** `feat(electronica2026): pdm_audio_eth frame protocol in asm, C and Rust`.

### Task 3: R5F ICSSG1 Ethernet init, SysConfig, ICSSG1 PRU projects

**Files:**
- Create: `mcuplus/eth_init.c`, `mcuplus/eth_init.h` (ported from the HIL `pru_eth_init.c/h`); `firmware/am243x-lp/icss_g1_pru0_fw/ti-pru-cgt/{example.projectspec,makefile,makefile_projectspec,linker.cmd}` and the same for `icss_g1_pru1_fw` (from the HIL projects, `OUTPUT_NAME` `pdm_audio_eth_am243x-lp_icss_g1_pru{0,1}_fw`, `MCU_HEX_NAME` `pr1_pru{0,1}_load_bin.h`, `HEX_ARRAY_PREFIX` `PR1_PRU{0,1}Firmware`, `CORE_ASM` `eth_tx.asm` / `eth_rx.asm`); stub `firmware/eth_tx.asm`, `firmware/eth_rx.asm` (each: set `ETH1_MAGIC` check, then `halt`)
- Modify: `example.syscfg`, `pdm_audio_eth_example.c`, `makefile` (add the two ICSSG1 firmware dirs), R5F makefile/projectspec include paths

**Interfaces:**
- Produces: `int32_t eth_init(void)` (0 ok, negative = HIL error codes −1..−6); `int32_t eth_wait_for_link(uint32_t timeout_ms)` (port 1 only); `pdm_audio_eth_example_main` order: ICSSG0 setup as `pdm_audio` → ICSSG1: halt all six cores, zero DRAM0/DRAM1/shared, write `ETH1_MAGIC/VERSION/CLIP_ADDR/CLIP_FRAMES`, C28 = 0x0100 for PRU0 and PRU1, `eth_init()`, `eth_wait_for_link(5000)`, load + start PRU1 then PRU0 → then ICSSG0 RTU1, PRU1, PRU0 as today. Console line `PDM audio eth: link 1000 FD on ICSSG1 port 1, board MAC 02:00:5d:4d:00:01` (or `link DOWN` and continue).
- SysConfig instance names: ICSSG0 = `CONFIG_PRU_ICSS0`, ICSSG1 = `CONFIG_PRU_ICSS1`, both `coreClk = 333333333`; ICSSG1 RGMII2 and MDIO pins assigned in SysConfig (so `Pinmux_init` sets the pads); `eth_init.c` drops the PADCONFIG/kick block of the HIL code (its kicks target 0x43001008/0x43005008 and write KICK1's value to the KICK0 register) and keeps steps 1 and 3–13 (MII_RT, G_CFG, GPCFG1, TX/RXCFG, IPG, G_RGMII_CFG 0x006600DD, IEP 0x331, MDIO, PHY ext regs).

- [ ] **Step 1:** Port and adapt `eth_init.c` as above; `eth_wait_for_link` wraps the HIL `PRU_ETH_wait_for_link(1, ms)`.
- [ ] **Step 2:** SysConfig: add the ICSSG1 instance and its RGMII2 (RXD0-3, RX_CTL, RX_CLK, TXD0-3, TX_CTL, TX_CLK) and MDIO/MDC pins, the pad values of HIL step 0 (RX input, TX output). Rename instances as above and update every `CONFIG_PRU_ICSS0/1` use in C.
- [ ] **Step 3:** Build (Global Constraints make line). Expected: exit 0, 5 PRU images, 0 new warnings; `clpru -v4` and the linter clean on both stubs.
- [ ] **Step 4: Board check (user).** PC cable on the LP's ICSSG1 port-1 RJ45; `nmcli device set enp129s0 managed no` once. Console shows the link line with 1000 FD; `ethtool enp129s0` shows `Speed: 1000Mb/s`, `Duplex: Full`, `Link detected: yes`. PDM checks of Task 1 Step 6 still pass.
- [ ] **Step 5: Commit** `feat(electronica2026): pdm_audio_eth R5F brings up ICSSG1 port 1 at 1 Gbit/s`.

### Task 4: ICSSG1 PRU1 receiver (filter, ping, throughput, control)

**Files:**
- Modify: `firmware/eth_rx.asm` (replace the stub)

**Interfaces:**
- Consumes: `eth_proto.inc` (Task 2), RX constants in "Protocol".
- Produces: ICSSG1 shared fields `PC_MAC`, `RX_*`, `PING_*`, `TPUT_*`, `REPORT_REQ` as in the map; behaviour per frame: not 0x88B5 or dst ≠ board MAC and ≠ broadcast → `RX_FOREIGN++`; bad version/type/length → `RX_BAD++`; else `RX_FRAMES++`, store src MAC; PING → store `PING_PC_NS`, `PING_RX_IEP` (IEP at the first bank), then `PING_REQ = seq + 1`; TPUT_DATA → `TPUT_RX_FRAMES++`, `TPUT_RX_BYTES += length`, gap if `seq ≠ last + 1`, first/last IEP; CONTROL 1 → zero `RX_*`/`TPUT_RX_*`; CONTROL 2 → `TPUT_TX_LEN = arg1; TPUT_TX_COUNT = arg0`; CONTROL 3 → `REPORT_REQ = 1`; AUDIO_IN → counted only (`RX_AUDIO++`) until Task 12.

- [ ] **Step 1:** Implement on the HIL RX loop (bank A/B, R18 count, EOF bit 20, reset 0xF4 after every frame including drops). Parse the header from the first bank (Ethernet 14 B + common header 16 B = first 30 B, all inside bank A's 32 B). Keep each bank's work under 85 cycles; note the worst path's cycle count in a comment.
- [ ] **Step 2:** `clpru -v4` 0 diagnostics; linter 0 errors; build the project.
- [ ] **Step 3: Commit** `feat(electronica2026): pdm_audio_eth ICSSG1 PRU1 frame filter, ping, throughput counters`.

### Task 5: ICSSG1 PRU0 transmitter (pong, throughput, report)

**Files:**
- Modify: `firmware/eth_tx.asm` (replace the stub)

**Interfaces:**
- Consumes: ICSSG1 shared fields from Task 4; `eth_proto.inc`.
- Produces: a main loop polling, in priority order, `PING_REQ` → PONG; `REPORT_REQ` → TPUT_REPORT; `TPUT_TX_COUNT` → that many TPUT_DATA frames of `TPUT_TX_LEN` (seq 0..n−1), back to back, then clear; (Task 10 adds audio first in the order). A TX macro/subroutine `send_frame(len)` that writes dst (PC_MAC), src (BOARD_MAC), EtherType, header (`ts` = IEP at start) and body from registers or local DRAM0 through TX_L2 in 64-byte XOUTs with the HIL FIFO-level wait, then CRC+EOF. Counters `TX_PONGS`. Frames are padded to at least 60 B before CRC.

- [ ] **Step 1:** Implement; nothing is sent until `mac_valid` is set.
- [ ] **Step 2:** `clpru -v4` 0 diagnostics; linter 0 errors; build.
- [ ] **Step 3: Commit** `feat(electronica2026): pdm_audio_eth ICSSG1 PRU0 pong, throughput and report frames`.

### Task 6: PC raw-Ethernet link

**Files:**
- Create: `pc_app/src/link.rs`

**Interfaces:**
- Produces: `pub trait Link: Send { fn send(&mut self, frame:&[u8]) -> io::Result<()>; fn recv(&mut self, timeout:Duration) -> io::Result<Option<RxFrame>>; fn mac(&self) -> [u8;6]; fn stats(&self) -> LinkStats; }`; `pub struct RxFrame { pub data: Vec<u8>, pub t_ns: u64 /* CLOCK_MONOTONIC, kernel timestamp */ }`; `pub struct LinkStats { pub rx_kernel_drops: u64 /* PACKET_STATISTICS tp_drops */ }`; `pub struct AfPacketLink` with `pub fn open(iface:&str) -> io::Result<Self>` (AF_PACKET/SOCK_RAW bound to `ETHERTYPE` on the interface, `SO_RCVBUF` 8 MiB via `SO_RCVBUFFORCE` falling back to `SO_RCVBUF`, `SO_TIMESTAMPNS`, interface MAC from `SIOCGIFHWADDR`); `pub fn link_info(iface:&str) -> io::Result<(u32 /*Mbit/s*/, bool /*full duplex*/, bool /*carrier*/)>` from `/sys/class/net/<iface>/{speed,duplex,carrier}`.
- Error text when the capability is missing: `raw socket on enp129s0 needs CAP_NET_RAW: run  sudo setcap cap_net_raw,cap_net_admin+ep <binary>  once after each build`.

- [ ] **Step 1: Failing tests:** `link_info_parses_sysfs` (temp dir with `speed` "1000", `duplex` "full", `carrier` "1" via an injectable root path `link_info_at(root, iface)`), `open_without_cap_gives_setcap_hint` (run when `getuid() != 0` and the binary lacks the cap: error string contains `setcap`).
- [ ] **Step 2:** `cargo test link` → fail.
- [ ] **Step 3:** Implement with `libc` (no extra crate).
- [ ] **Step 4:** `cargo test link` → pass.
- [ ] **Step 5: Commit** `feat(pdm_audio_eth): pc_app AF_PACKET link`.

### Task 7: Board emulator

**Files:**
- Create: `pc_app/src/emulator.rs`

**Interfaces:**
- Consumes: `proto` (Task 2), `Link` (Task 6).
- Produces: `pub struct Emulator` implementing `Link`, `pub fn new(cfg: EmuConfig) -> Self`; `pub struct EmuConfig { pub audio_out: bool, pub clip_frames: u32 /*16000*/, pub pipeline_frames: u32 /*PCM delay vs RTU1, default 40*/, pub drop_every_nth_out: Option<u32>, pub turnaround_ns: u64 }`. It runs on wall-clock time: RTU1 consumes its ring at 16 kHz, emits two AUDIO_OUT per 64 frames with a correct status block (PCM = ring sample << 8, delayed by `pipeline_frames`), answers PING / CONTROL / TPUT like Tasks 4–5 and stores AUDIO_IN into its ring at `first_index mod clip_frames`; frames with another EtherType are counted in `rx_foreign` and otherwise ignored. `pub fn reset_board(&mut self)` sets HALF_SEQ, RTU1 position and counters back to 0 (for resync tests).

- [ ] **Step 1: Failing tests:** `emulator_answers_ping`, `emulator_tput_tx_sends_count_frames`, `emulator_audio_out_500_per_second` (over 1.0 s: 500 ± 5 frames, `first_index` steps of 32), `emulator_ignores_foreign_ethertype` (rx_foreign counts it, no reply), `emulator_audio_in_lands_in_ring`.
- [ ] **Step 2:** `cargo test emulator` → fail. **Step 3:** implement. **Step 4:** pass.
- [ ] **Step 5: Commit** `feat(pdm_audio_eth): pc_app board emulator`.

### Task 8: Ethernet test (CLI) + stage 1 board run

**Files:**
- Create: `pc_app/src/ethtest.rs`; Modify: `pc_app/src/main.rs` (`pdm-eth test [--iface enp129s0] [--emulator] [--pings 10000] [--seconds 3]`)

**Interfaces:**
- Consumes: `Link`, `Emulator`, `proto`.
- Produces: `pub struct PingResult { sent:u32, lost:u32, min_us:f64, mean_us:f64, p99_us:f64, max_us:f64, board_turnaround_ns: (u64,u64) /*min,max of tx_iep - rx_iep*/ }`; `pub struct TputResult { dir: Dir, frames:u64, bytes:u64, mbit_s:f64, seq_gaps:u64, pc_kernel_drops:u64 }`; `pub struct Summary { link: (u32,bool,bool), ping: PingResult, to_board: TputResult, from_board: TputResult }` with `fn verdicts(&self) -> Vec<(&'static str, bool, String)>` using the Global Constraints thresholds; `pub fn run(link:&mut dyn Link, cfg:&TestConfig) -> Summary`. PC → board throughput: send `TPUT_DATA` of 1500-byte payload for `seconds`, then CONTROL 3, read TPUT_REPORT, rate from `first_iep/last_iep`. Board → PC: CONTROL 2 with count = `seconds x 81274`, length 1500, count received.

- [ ] **Step 1: Failing tests:** `ping_stats_against_emulator` (1000 pings, lost 0, min ≤ mean ≤ p99 ≤ max), `ping_counts_lost` (emulator drops every 10th pong → lost 100), `verdicts_apply_thresholds` (499 Mbit/s fails, 500 passes; 0.1 % loss fails), `tput_report_splits_pc_and_board_loss` (board TPUT_REPORT frames = sent and PC `seq_gaps` > 0 with `pc_kernel_drops` > 0 → verdict text says "PC dropped", not "board").
- [ ] **Step 2:** fail. **Step 3:** implement; `main.rs` prints one line per verdict: `PASS link 1000/full`, `PASS ping 10000/10000, rtt min/mean/p99/max 41/52/80/310 us, board turnaround 1.2-1.9 us`, …
- [ ] **Step 4:** `cargo test` pass; `cargo run --release -- test --emulator` prints all PASS.
- [ ] **Step 5: Board run (stage 1 checkpoint).** User: `! sudo setcap cap_net_raw,cap_net_admin+ep pc_app/target/release/pdm-eth` once per build, board loaded and running (Task 3–5 firmware). Run `pc_app/target/release/pdm-eth test --iface enp129s0`. Record the full output in the readme draft (Task 16). Fix firmware until every line is PASS or the threshold is consciously revised in the design with the measured value.
- [ ] **Step 6: Commit** `feat(pdm_audio_eth): pc_app Ethernet test (link, ping, throughput)`.

## Stage 2: Board → PC audio

### Task 9: PC reconstruction and capture

**Files:**
- Create: `pc_app/src/rx.rs`; Modify: `main.rs` (`pdm-eth capture --seconds N --out DIR [--emulator]`)

**Interfaces:**
- Produces: `pub struct OutStream` with `pub fn push(&mut self, status:&Status, pcm:&[[i32;8];32], tx_iep:u64, pc_rx_ns:u64)`; `pub fn take_new(&mut self) -> Vec<(u64 /*index*/, [i32;8])>` (contiguous, gaps filled with zeros); counters `pub lost_frames:u64, pub duplicates:u64, pub resyncs:u64`; `pub fn board_clock_ppm(&self) -> Option<f64>` (board IEP vs PC monotonic over the last 10 s); `pub fn last_status(&self) -> Option<&Status>`. `pub struct WavCapture` `fn create(dir:&Path, prefix:&str) -> io::Result<Self>` (8 files `out_ch{0..7}.wav`, 16 kHz, mono, 24-bit, value = i32 >> 0 clamped to 24 bits) and `fn write(&mut self, frames:&[(u64,[i32;8])])`, `fn finish(self)`.

- [ ] **Step 1: Failing tests:** `rx_places_by_index` (out-of-order part 1 before part 0 → correct order), `rx_counts_gap_and_fills_silence` (missing one frame → lost 1, 32 zero frames), `rx_ignores_duplicate`, `rx_resync_on_index_jump_back` (index drops from 1 000 000 to 0 → resyncs 1, lost unchanged, stream continues), `capture_writes_eight_24bit_files` (hound reads back 16 kHz, 1 channel, 24 bit, same samples).
- [ ] **Step 2:** fail. **Step 3:** implement. **Step 4:** pass; `pdm-eth capture --emulator --seconds 2 --out /tmp/x` writes 8 files of 32 000 samples.
- [ ] **Step 5: Commit** `feat(pdm_audio_eth): pc_app AUDIO_OUT reconstruction and WAV capture`.

### Task 10: ICSSG1 PRU0 audio out

**Files:**
- Modify: `firmware/eth_tx.asm`

**Interfaces:**
- Consumes: ICSSG0 header and halves at global 0x30010000 (`pdm8_shared.inc` offsets), ICSSG1 shared map.
- Produces: top-priority audio path in the PRU0 loop: on a new HALF_SEQ (read via global 0x30010018), record `detect_iep`, copy the 2048 B half into ICSSG1 PRU0 DRAM0 (local 0x0000..0x07FF) in LBBO/SBBO bursts of 32 B or more, read the ICSSG0 counters (loops/pos twice until both reads agree), then send part 0 and part 1 as AUDIO_OUT (`seq` per AUDIO_OUT frame). If HALF_SEQ advanced by more than one since the last send, `TX_LATE++` and send only the newest half. `TX_AUDIO++` per frame. Pongs and reports wait while an audio pair is in flight; a running TPUT_TX burst yields to audio between frames.

- [ ] **Step 1:** Implement; comment the cycle budget (copy time, two frames ~9 µs each at wire rate, the 4 ms per half).
- [ ] **Step 2:** `clpru -v4` 0 diagnostics; linter 0 errors; build.
- [ ] **Step 3: Board run.** `pdm-eth capture --iface enp129s0 --seconds 600 --out ~/pdm_capture`: `lost_frames 0`, `resyncs 0`; `tx_late` 0 in the last status; tone channels 3–7 of the WAVs: 1000, 3000, 500, 2000, 5000 Hz (± 1 Hz by FFT) at the `pdm_audio` levels (1.000, 0.942, 0.999, 0.981, 0.850 x 2^22 ± 2 %); one CCS read of half A matches the frames with the same index in the capture.
- [ ] **Step 4: Commit** `feat(electronica2026): pdm_audio_eth ICSSG1 PRU0 streams the PCM halves`.

### Task 11: Spectrograms and the layout-C window (output side)

**Files:**
- Create: `pc_app/src/spectro.rs`, `pc_app/src/ui.rs`; Modify: `main.rs` (`pdm-eth gui [--iface enp129s0 | --emulator]`)

**Interfaces:**
- Consumes: `OutStream` (Task 9), `Link`.
- Produces: `pub struct Spectrogram` `fn new(fft:usize /*256*/, hop:usize /*128*/, min_db:f32 /*-90*/, max_db:f32 /*0*/, full_scale:f32)`, `fn push(&mut self, samples:&[f32])`, `fn columns(&self) -> &VecDeque<Vec<f32>>` (129 bins, dB clamped), capacity 1250 columns (10 s); `pub fn diff_column(a:&[f32], b:&[f32]) -> Vec<f32>` (per-bin dB difference). `ui::App` (eframe) with a network thread owning the `Link` and an `OutStream`, sending `(index, [i32;8])` blocks over a channel; the UI thread feeds 8 output `Spectrogram`s (full scale 2^23) and draws layout C: channel selector (0–7), output spectrogram (input pane shows "no input" until Task 14), side panel (fps, frames/s out, lost, UNDERRUN_COUNT, SD_OVF_COUNT, tx_late, board clock ppm, "simulation: about 11 min per second of audio"), link state.

- [ ] **Step 1: Failing tests:** `spectro_tone_peak_bin` (1 kHz at 16 kHz → peak bin 16 of 129), `spectro_hop_columns` (16 000 samples → 123 columns), `spectro_db_clamped`, `diff_column_zero_for_equal`, `spectro_8ch_realtime_budget` (8 channels x 1 s of audio processed in < 50 ms in `--release`, `#[ignore]` in debug).
- [ ] **Step 2:** fail. **Step 3:** implement (`rustfft`, Hann precomputed; texture upload of only new columns per frame).
- [ ] **Step 4:** pass; `pdm-eth gui --emulator` shows a scrolling spectrogram and fps ≥ 60 in the side panel.
- [ ] **Step 5: Board run (stage 2 checkpoint).** `pdm-eth gui --iface enp129s0`: channel 3 shows a 1 kHz line, channel 1 the chirp; fps ≥ 30 while switching channels; lost 0 over 10 minutes.
- [ ] **Step 6: Commit** `feat(pdm_audio_eth): pc_app live output spectrograms, layout C`.

## Stage 3: PC → board audio

### Task 12: ICSSG1 PRU1 audio in

**Files:**
- Modify: `firmware/eth_rx.asm`

**Interfaces:**
- Consumes: `ETH1_CLIP_ADDR`, `ETH1_CLIP_FRAMES` (written by the R5F, Task 3).
- Produces: AUDIO_IN with `first_index` a multiple of 64 → the 1024 B go to global `CLIP_ADDR + (first_index mod CLIP_FRAMES) x 16` (`CLIP_FRAMES` 16 000 is a multiple of 64, so a frame never wraps); `LAST_IN_END = first_index + 64`; `RX_AUDIO++`. Other `first_index` values → `RX_BAD++`. Store straight from the RX banks or buffer locally and copy after EOF; whichever is chosen, a back-to-back pair of AUDIO_IN frames must both land (checked on the board).

- [ ] **Step 1:** Implement; comment the path chosen and its cycle count per bank.
- [ ] **Step 2:** `clpru -v4` 0 diagnostics; linter 0 errors; build.
- [ ] **Step 3: Commit** `feat(electronica2026): pdm_audio_eth ICSSG1 PRU1 writes PC audio into the clip buffer`.

### Task 13: PC input: WAV loading and pacing

**Files:**
- Create: `pc_app/src/wavin.rs`, `pc_app/src/pacing.rs`; Modify: `main.rs` (`pdm-eth stream --ch0 a.wav … --ch7 h.wav [--capture DIR] [--iface | --emulator]`)

**Interfaces:**
- Produces: `pub fn load_channel(path:&Path) -> Result<Vec<i16>, WavError>` (any rate → 16 kHz with `rubato`, stereo/multi → mean to mono, int8/16/24/32/float → f32, peak-normalised to 16384 = 0.5 FS; `WavError` message names the file); `pub struct InputSet { pub ch: [Vec<i16>; 8] }` with `fn frames(&self) -> usize` (longest) and `fn frame(&self, i:usize) -> [i16;8]` (silence past a file's end). `pub struct Pacer` `fn new(lead_frames:u32 /*8000*/, clip_frames:u32)`, `fn on_status(&mut self, s:&Status, pc_ns:u64)`, `fn next_due(&mut self, pc_ns:u64) -> Option<u32 /*first_index*/>` (returns the next 64-frame block while `index − rtu1_total_est < lead`; first block = round_up(rtu1_total + lead, 64)), counters `pub underruns:u64` (RTU1 passed `last_in_end`), `pub overwrite_blocked:u64` (a block would be more than `clip_frames − 64` ahead), `pub restarts:u64`.

- [ ] **Step 1: Failing tests:** `wav_48k_stereo_float_to_16k_mono`, `wav_24bit_and_8bit_load`, `wav_peak_is_16384`, `wav_short_file_pads_silence`, `wav_empty_or_missing_reports_file`, `pacing_starts_lead_ahead`, `pacing_holds_lead_at_16khz` (simulated 10 s of statuses: lead stays within 8000 ± 128), `pacing_counts_underrun_and_recovers` (no sends for 0.6 s → underruns ≥ 1, lead back to 8000 ± 128 within 1 s), `pacing_never_writes_past_read_position` (no block more than 15 936 frames ahead), `pacing_restarts_after_board_reset` (`rtu1_total` drops to 0 → restarts 1, next block = round_up(0 + 8000, 64)).
- [ ] **Step 2:** fail. **Step 3:** implement; the sender runs in the network thread, at most one AUDIO_IN per `next_due`.
- [ ] **Step 4:** pass; `pdm-eth stream --emulator --ch0 clips/speech_librivox.wav --capture /tmp/y` runs, `underruns 0`, and `/tmp/y` holds `out_ch{0..7}.wav` (24-bit) and `in_ch{0..7}.wav` (16-bit, the converted input as sent, same 16 kHz timeline).
- [ ] **Step 5: Commit** `feat(pdm_audio_eth): pc_app WAV input and pacing against the RTU1 position`.

### Task 14: Input pane, difference strip, board latency + stage 3 board run

**Files:**
- Create: `pc_app/src/latency.rs`; Modify: `pc_app/src/ui.rs`, `main.rs` (`gui` gains `--ch0..--ch7`)

**Interfaces:**
- Consumes: `InputSet`, `Pacer` (Task 13), `OutStream` (Task 9), `Spectrogram`, `diff_column` (Task 11).
- Produces: `pub struct LatencyMeter` `fn push_in(&mut self, index:u64, x:&[i16])`, `fn push_out(&mut self, index:u64, y:&[i32], tx_iep:u64)`, `fn push_status(&mut self, s:&Status)`, `fn board_latency_ms(&self) -> Option<f64>`: lag L (frames) from cross-correlating input and output of the selected channel over the last 2 s (search 0..1024); input sample i was taken by RTU1 at `t_in(i) = detect_iep + (i − rtu1_total) / 16 kHz` (from the newest status), output sample `i + L` left the board at the `ts` of its AUDIO_OUT frame; latency = `t_out − t_in` in ms. `None` without input or while the correlation peak is under 0.5 (normalised). UI: input spectrogram (full scale 32768) on top, output below, difference strip, side panel adds "board audio latency", "buffered ahead (PC)" (= lead / 16 kHz), frames/s in, input underruns.

- [ ] **Step 1: Failing tests:** `latency_emulator_known_delay` (emulator `pipeline_frames` 40, zero network delay → latency 2.5 ms ± 0.2 ms), `latency_none_without_input`, `latency_none_when_uncorrelated` (noise input, unrelated output).
- [ ] **Step 2:** fail. **Step 3:** implement. **Step 4:** pass; `pdm-eth gui --emulator --ch0 clips/speech_librivox.wav` shows both panes, a near-empty difference strip, latency ≈ 2.5 ms.
- [ ] **Step 5: Board run (stage 3 checkpoint).** Eight WAVs of ≥ 60 s (one per channel; the booth clips may be looped into longer files with `sox` for the test) via `pdm-eth gui --iface enp129s0 --ch0 … --ch7 …`, and once `pdm-eth stream … --capture ~/pdm_loop`: input underruns 0, overwrite_blocked 0, lost 0, UNDERRUN_COUNT 0, SD_OVF_COUNT 0; output spectrograms match the inputs; the board latency number is shown and stable within ± 1 ms; record it.
- [ ] **Step 6: Commit** `feat(pdm_audio_eth): pc_app input/output side by side and board audio latency`.

### Task 15: Processing seam

**Files:**
- Create: `pc_app/src/seam.rs`; Modify: `ui.rs` (optional third pane when a stage is active), `main.rs` (`--export-shm NAME`)

**Interfaces:**
- Produces: `pub trait Stage: Send { fn name(&self) -> &str; fn process(&mut self, index:u64, input:&[[i16;8]], output:&[[i32;8]]) -> Option<Vec<[f32;8]>>; }` called with aligned 64-frame blocks; `pub struct Identity` (returns the output as f32, for the test); the window shows a stage's result as a third spectrogram and its mean `process` time per block. `pub struct ShmExport` `fn create(name:&str, blocks:usize) -> io::Result<Self>`, `fn write(&mut self, index:u64, input:&[[i16;8]], output:&[[i32;8]])`: a POSIX shared-memory ring `/dev/shm/<name>` with a 64-byte header (magic `PDMSHM01`, block count, write counter u64) followed by blocks of `u64 index + 64x8 i16 + 64x8 i32`; layout documented in the module doc comment for the later Python/CUDA reader.

- [ ] **Step 1: Failing tests:** `identity_stage_sees_aligned_blocks` (input index i and output index i in the same call after the latency lag is applied), `shm_ring_wraps_and_counts` (write blocks+3, counter = blocks+3, oldest overwritten).
- [ ] **Step 2:** fail. **Step 3:** implement. **Step 4:** pass.
- [ ] **Step 5: Commit** `feat(pdm_audio_eth): pc_app processing seam (stage trait, shared-memory export)`.

### Task 16: Readme, examples list, final verification

**Files:**
- Create: `pdm_audio_eth/pdm_audio_eth/readme.md`, `pc_app/README.md`
- Modify: `examples/makefile` (SUBDIRS + `electronica2026/pdm_audio_eth/pdm_audio_eth`), `examples/readme.md` (project list + portability row `Y Yport Yport Yport Yport Yport`, as `pdm_audio`)

**Interfaces:**
- Consumes: results recorded in Tasks 1, 3, 8, 10, 11, 14.

- [ ] **Step 1:** Readme in the `pdm_audio` style: what it does, supported combinations, validated HW/SW, wiring (the nine PDM jumpers as `pdm_audio`, the RJ45 of ICSSG1 port 1, direct cable to `enp129s0`), PC setup (`nmcli … managed no`, `setcap` after every build), build and run, the three stages with their CLI commands, hardware results (stage 1 test output, stage 2 capture, stage 3 latency), known limits (replay of old audio if the PC falls behind more than the lead; PC-side loss in throughput tests), and a link to `docs/design.md`. `pc_app/README.md`: build, `setcap`, commands, emulator.
- [ ] **Step 2: Final verification:** `tools/check_icssg0_copy.sh` exit 0; `make` (Global Constraints line) exit 0 with only the SysConfig notice; CCS headless `projectCreate` + Release build of the 6 projects (fresh workspace, `ccs-server-cli.sh`), 0 errors; `clpru -v4` and the linter clean on `eth_tx.asm`, `eth_rx.asm`; `cargo test` and `cargo clippy -- -D warnings` clean in `pc_app/`; `git status` shows no build output staged and the three untracked other-branch folders untouched.
- [ ] **Step 3: Commit** `docs(electronica2026): pdm_audio_eth readme, examples list and portability row`.
