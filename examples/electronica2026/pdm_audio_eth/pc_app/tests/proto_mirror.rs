// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! The protocol exists three times: firmware/eth_proto.inc (PRU),
//! mcuplus/eth_proto.h (R5F) and src/proto.rs (PC). They must agree.

use std::collections::BTreeMap;
use std::path::PathBuf;

fn board_file(rel: &str) -> String {
    let p = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../pdm_audio_eth").join(rel);
    std::fs::read_to_string(&p).unwrap_or_else(|e| panic!("{}: {e}", p.display()))
}

fn parse_num(v: &str) -> u64 {
    let v = v.trim().trim_end_matches('U').trim_end_matches('u');
    match v.strip_prefix("0x").or_else(|| v.strip_prefix("0X")) {
        Some(h) => u64::from_str_radix(h, 16).unwrap(),
        None => v.parse().unwrap(),
    }
}

fn is_mirrored(name: &str) -> bool {
    name.starts_with("ETH_") || name.starts_with("ETH1_")
}

/// `NAME .set VALUE ; comment`
fn parse_inc(text: &str) -> BTreeMap<String, u64> {
    let mut m = BTreeMap::new();
    for line in text.lines() {
        let code = line.split(';').next().unwrap();
        let t: Vec<&str> = code.split_whitespace().collect();
        if t.len() >= 3 && t[1] == ".set" && is_mirrored(t[0]) {
            assert!(m.insert(t[0].to_string(), parse_num(t[2])).is_none(), "duplicate {}", t[0]);
        }
    }
    m
}

/// `#define NAME (VALUE)` / `#define NAME VALUE`
fn parse_h(text: &str) -> BTreeMap<String, u64> {
    let mut m = BTreeMap::new();
    for line in text.lines() {
        let code = line.split("/*").next().unwrap();
        let t: Vec<&str> = code.split_whitespace().collect();
        if t.len() >= 3 && t[0] == "#define" && is_mirrored(t[1]) {
            let v = t[2].trim_start_matches('(').trim_end_matches(')');
            assert!(m.insert(t[1].to_string(), parse_num(v)).is_none(), "duplicate {}", t[1]);
        }
    }
    m
}

#[test]
fn inc_h_rs_agree() {
    let inc = parse_inc(&board_file("firmware/eth_proto.inc"));
    let h = parse_h(&board_file("mcuplus/eth_proto.h"));
    let rs: BTreeMap<String, u64> =
        pdm_eth::proto::mirror::ALL.iter().map(|(n, v)| (n.to_string(), *v)).collect();
    assert_eq!(rs.len(), pdm_eth::proto::mirror::ALL.len(), "duplicate name in mirror::ALL");
    assert!(!rs.is_empty());
    assert_eq!(inc, rs, "eth_proto.inc differs from proto.rs");
    assert_eq!(h, rs, "eth_proto.h differs from proto.rs");
}
