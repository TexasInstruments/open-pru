// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! Structural checks of the ICSSG1 firmware that the PC tests can make
//! without a board: handlers that must not fall through into the next one.

use std::path::PathBuf;

fn asm(name: &str) -> Vec<String> {
    let p = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../pdm_audio_eth/firmware").join(name);
    std::fs::read_to_string(&p)
        .unwrap_or_else(|e| panic!("{}: {e}", p.display()))
        .lines()
        .map(|l| l.split(';').next().unwrap().trim().to_string())
        .collect()
}

/// The last instruction before `label:` (skipping blank lines and labels).
fn before(lines: &[String], label: &str) -> String {
    let i = lines.iter().position(|l| l == &format!("{label}:")).unwrap_or_else(|| panic!("no {label}"));
    lines[..i].iter().rev().find(|l| !l.is_empty() && !l.ends_with(':')).cloned().unwrap()
}

fn unconditional(insn: &str) -> bool {
    let op = insn.split_whitespace().next().unwrap_or("");
    matches!(op, "qba" | "jmp" | "halt")
}

#[test]
fn rx_handlers_do_not_fall_through() {
    let l = asm("eth_rx.asm");
    // entered only by branches; the code before each must end with a jump
    for label in ["b_ain", "c_reset", "c_tput_tx", "c_report", "b_ping", "a_ping", "a_ctl", "a_audio", "a_tput",
                  "ain_part_a", "ain_part_b", "ain_eof_a"] {
        let b = before(&l, label);
        assert!(unconditional(&b), "eth_rx.asm: '{b}' falls through into {label}");
    }
}

#[test]
fn tx_routines_do_not_fall_through() {
    let l = asm("eth_tx.asm");
    for label in ["do_pong", "do_report", "do_tput", "audio", "build_header", "send_frame"] {
        let b = before(&l, label);
        assert!(unconditional(&b), "eth_tx.asm: '{b}' falls through into {label}");
    }
}

/// ICSSG IEP0 (AM64x/AM243x TRM, PRU_ICSSG IEP registers): global_cfg 0x00,
/// global_status 0x04, compen 0x08, slow_compen 0x0C, count_reg0 0x10,
/// count_reg1 0x14. 0x0C is the count on the older PRU-ICSS, not on ICSSG
/// (board run 2026-11-05: timestamps took slow_compen as their low word).
#[test]
fn iep_count_is_icssg_count_reg0() {
    let inc = std::fs::read_to_string(PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../pdm_audio_eth/firmware/eth_proto.inc")).unwrap();
    let val = |name: &str| {
        let l = inc.lines().find(|l| l.split_whitespace().next() == Some(name)).unwrap_or_else(|| panic!("no {name}"));
        let v = l.split_whitespace().nth(2).unwrap();
        u32::from_str_radix(v.trim_start_matches("0x"), 16).unwrap()
    };
    assert_eq!(val("ICSS_IEP_COUNT_LO"), 0x10);
    assert_eq!(val("ICSS_IEP_COUNT_HI"), 0x14);
    let c = std::fs::read_to_string(PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../pdm_audio_eth/mcuplus/eth_init.c")).unwrap();
    assert!(c.contains("ICSSG1_IEP_COUNT_LO         (ICSSG1_BASE + 0x0002E010U)"), "eth_init.c COUNT_LO");
    assert!(c.contains("ICSSG1_IEP_COUNT_HI         (ICSSG1_BASE + 0x0002E014U)"), "eth_init.c COUNT_HI");
}

/// After an audio pair PRU0 must pause before the next frame: without it the
/// TPUT frame that followed the second AUDIO_OUT sometimes never became a
/// valid frame on the wire (board run 2026-11-05: 2 to 6 per 243 822-frame
/// burst; the MAC counted it as sent, the PC saw nothing, even promiscuous).
/// 50 x 2 cycles (0.3 us) removed the loss over 1.46 M frames.
#[test]
fn audio_pair_ends_with_tx_guard() {
    let raw = std::fs::read_to_string(PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../pdm_audio_eth/firmware/eth_tx.asm")).unwrap();
    let guard = raw.lines().find_map(|l| {
        let t: Vec<&str> = l.split(';').next().unwrap().split_whitespace().collect();
        (t.len() >= 3 && t[0] == "AUDIO_TX_GUARD" && t[1] == ".set").then(|| t[2].parse::<u32>().unwrap())
    });
    assert!(guard.is_some_and(|g| g >= 50), "AUDIO_TX_GUARD .set >= 50 missing: {guard:?}");
    let l = asm("eth_tx.asm");
    let i = l.iter().position(|x| x == "audio_done:").expect("audio_done");
    let body: Vec<&String> = l[i + 1..].iter().filter(|x| !x.is_empty()).take(4).collect();
    assert!(body[0].starts_with("ldi") && body[0].contains("AUDIO_TX_GUARD"), "audio_done starts with {:?}", body[0]);
}
