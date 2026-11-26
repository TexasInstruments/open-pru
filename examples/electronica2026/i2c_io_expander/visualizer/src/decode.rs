// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! Parsing of PicoScope 7 I2C decoder group keys and choice of the LED value.
//!
//! A `groupBy:"Data"` key holds the bytes after the address as space-separated
//! hex, for example `"01 08 "` = register 0x01 (Output Port), data 0x08.

/// Output Port register of the TCA9538.
const OUTPUT_PORT_REG: u8 = 0x01;

/// Parse a key of space-separated two-digit hex bytes. `None` if any token
/// is not exactly two hex digits.
fn hex_bytes(key: &str) -> Option<Vec<u8>> {
    key.split_whitespace()
        .map(|t| {
            if t.len() == 2 {
                u8::from_str_radix(t, 16).ok()
            } else {
                None
            }
        })
        .collect()
}

/// Data byte of a valid Output Port frame (`"01 xx"`), else `None`.
pub fn parse_frame_key(key: &str) -> Option<u8> {
    match hex_bytes(key)?.as_slice() {
        [OUTPUT_PORT_REG, data] => Some(*data),
        _ => None,
    }
}

/// Address from a `groupBy:"Address"` key such as `"23 "`.
pub fn parse_address_key(key: &str) -> Option<u8> {
    match hex_bytes(key)?.as_slice() {
        [addr] => Some(*addr),
        _ => None,
    }
}

/// Next value of the running-LED pattern (0x80 wraps to 0x01).
pub fn successor(v: u8) -> u8 {
    v.rotate_left(1)
}

/// What the latest capture means for the shown LED value.
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum Pick {
    /// Show this value.
    New(u8),
    /// Valid frames, but none can be chosen safely: keep the shown value.
    Keep,
    /// No valid frame in the capture.
    NoFrame,
}

/// Choose the value to show from the valid frames of one capture.
///
/// One frame is shown as is. With several frames (not expected with the
/// booth trigger), only the successor of the shown value is taken; anything
/// else keeps the shown value, so no value is ever invented.
pub fn pick(prev: Option<u8>, candidates: &[u8]) -> Pick {
    match (candidates, prev) {
        ([], _) => Pick::NoFrame,
        ([only], _) => Pick::New(*only),
        (many, Some(p)) if many.contains(&successor(p)) => Pick::New(successor(p)),
        _ => Pick::Keep,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_valid_frame() {
        assert_eq!(parse_frame_key("01 08 "), Some(0x08));
        assert_eq!(parse_frame_key("01 80"), Some(0x80));
    }

    #[test]
    fn parse_rejects_partial() {
        assert_eq!(parse_frame_key("01 "), None);
    }

    #[test]
    fn parse_rejects_config_frame() {
        assert_eq!(parse_frame_key("03 00 "), None);
    }

    #[test]
    fn parse_rejects_three_bytes_and_junk() {
        assert_eq!(parse_frame_key("01 08 09 "), None);
        assert_eq!(parse_frame_key("zz 01 "), None);
        assert_eq!(parse_frame_key(""), None);
    }

    #[test]
    fn parse_address() {
        assert_eq!(parse_address_key("23 "), Some(0x23));
        assert_eq!(parse_address_key("x"), None);
    }

    #[test]
    fn successor_wraps() {
        assert_eq!(successor(0x01), 0x02);
        assert_eq!(successor(0x80), 0x01);
    }

    #[test]
    fn pick_none() {
        assert_eq!(pick(Some(0x04), &[]), Pick::NoFrame);
    }

    #[test]
    fn pick_single() {
        assert_eq!(pick(Some(0x04), &[0x08]), Pick::New(0x08));
        assert_eq!(pick(None, &[0x20]), Pick::New(0x20));
    }

    #[test]
    fn pick_single_same_is_new() {
        assert_eq!(pick(Some(0x08), &[0x08]), Pick::New(0x08));
    }

    #[test]
    fn pick_many_takes_successor() {
        assert_eq!(pick(Some(0x04), &[0x40, 0x08]), Pick::New(0x08));
    }

    #[test]
    fn pick_many_without_successor_keeps() {
        assert_eq!(pick(Some(0x04), &[0x40, 0x10]), Pick::Keep);
        assert_eq!(pick(None, &[0x01, 0x02]), Pick::Keep);
    }
}
