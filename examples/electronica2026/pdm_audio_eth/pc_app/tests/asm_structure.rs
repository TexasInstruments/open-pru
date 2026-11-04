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
