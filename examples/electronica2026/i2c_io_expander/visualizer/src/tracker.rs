// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! Connection and LED state, fed by one poll of PicoScope 7 at a time.

use crate::decode::{Pick, parse_address_key, parse_frame_key, pick, successor};
use crate::mcp::{McpError, ToolCaller};
use crate::setup::ensure_decoder;
use serde_json::{Value, json};
use std::time::{Duration, Instant};

/// No valid frame for this long: `NoFrames`.
const NO_FRAMES_AFTER: Duration = Duration::from_secs(2);
/// Address query period.
const ADDRESS_EVERY: Duration = Duration::from_secs(1);

/// Connection state shown in the window.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Conn {
    /// PicoScope 7 not reachable: all LEDs dark.
    Waiting,
    /// PicoScope answers and frames arrive.
    Live,
    /// PicoScope answers, but no valid frame for 2 s: last LED kept.
    NoFrames,
}

/// What the GUI draws.
#[derive(Debug, Clone, PartialEq)]
pub struct Snapshot {
    pub conn: Conn,
    pub value: Option<u8>,
    pub address: Option<u8>,
}

impl Snapshot {
    /// LED P0..P7 on/off.
    pub fn lit(&self) -> [bool; 8] {
        let v = match (self.conn, self.value) {
            (Conn::Waiting, _) | (_, None) => 0,
            (_, Some(v)) => v,
        };
        std::array::from_fn(|i| v >> i & 1 == 1)
    }

    /// Status line under the board.
    pub fn status_text(&self) -> String {
        match (self.conn, self.value) {
            (Conn::Waiting, _) => "waiting for PicoScope 7 …".into(),
            (Conn::NoFrames, _) => "no I2C frames".into(),
            (Conn::Live, None) => "waiting for I2C frames …".into(),
            (Conn::Live, Some(v)) => {
                let addr = self.address.map_or("?".to_string(), |a| format!("0x{a:02x}"));
                format!("last frame: addr {addr} · reg 0x01 · data 0x{v:02x}")
            }
        }
    }
}

/// Result of one poll.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct PollResult {
    /// The LED value changed.
    pub changed: bool,
    /// The change was not the next step of the pattern.
    pub skipped: bool,
}

pub struct Tracker {
    conn: Conn,
    value: Option<u8>,
    address: Option<u8>,
    /// Decoder id; `None` means run [`ensure_decoder`] on the next poll.
    decoder: Option<String>,
    /// Time of the last reply with a valid frame, or of going Live.
    last_frame: Option<Instant>,
    last_address_query: Option<Instant>,
}

impl Default for Tracker {
    fn default() -> Self {
        Self::new()
    }
}

impl Tracker {
    pub fn new() -> Self {
        Tracker {
            conn: Conn::Waiting,
            value: None,
            address: None,
            decoder: None,
            last_frame: None,
            last_address_query: None,
        }
    }

    pub fn snapshot(&self) -> Snapshot {
        Snapshot { conn: self.conn, value: self.value, address: self.address }
    }

    /// Poll PicoScope 7 once and update the state.
    pub fn poll_once(&mut self, c: &mut dyn ToolCaller, now: Instant) -> PollResult {
        match self.try_poll(c, now) {
            Ok(r) => r,
            Err(_) => {
                self.conn = Conn::Waiting;
                self.decoder = None;
                PollResult::default()
            }
        }
    }

    fn summary(c: &mut dyn ToolCaller, id: &str, group_by: &str) -> Result<Value, McpError> {
        c.call_tool("get_serial_decoder_summary", json!({"id":id,"groupBy":group_by,"topK":5}))
    }

    fn try_poll(&mut self, c: &mut dyn ToolCaller, now: Instant) -> Result<PollResult, McpError> {
        let id = match &self.decoder {
            Some(id) => id.clone(),
            None => {
                let id = ensure_decoder(c)?;
                self.decoder = Some(id.clone());
                id
            }
        };
        let reply = Self::summary(c, &id, "Data")?;
        if reply["status"] != "Success" {
            // Decoder gone or not ready: find it again next time.
            self.decoder = None;
            return Ok(PollResult::default());
        }
        if self.conn == Conn::Waiting {
            self.conn = Conn::Live;
            self.last_frame = Some(now);
        }

        let keys = group_keys(&reply);
        let candidates: Vec<u8> = keys.iter().filter_map(|k| parse_frame_key(k)).collect();
        let prev = self.value;
        let mut result = PollResult::default();
        match pick(prev, &candidates) {
            Pick::New(v) => {
                result.changed = prev != Some(v);
                result.skipped = result.changed && prev.is_some_and(|p| successor(p) != v);
                self.value = Some(v);
                self.last_frame = Some(now);
                self.conn = Conn::Live;
            }
            Pick::Keep => {
                self.last_frame = Some(now);
                self.conn = Conn::Live;
            }
            Pick::NoFrame => {
                if self.last_frame.is_some_and(|t| now.duration_since(t) >= NO_FRAMES_AFTER) {
                    self.conn = Conn::NoFrames;
                }
            }
        }

        if self.last_address_query.is_none_or(|t| now.duration_since(t) >= ADDRESS_EVERY) {
            self.last_address_query = Some(now);
            // Best effort: an address error leaves the address as it was.
            if let Ok(r) = Self::summary(c, &id, "Address") {
                if let Some(a) = group_keys(&r).iter().find_map(|k| parse_address_key(k)) {
                    self.address = Some(a);
                }
            }
        }
        Ok(result)
    }
}

/// Group keys of a `get_serial_decoder_summary` reply.
fn group_keys(reply: &Value) -> Vec<String> {
    reply["result"]["groups"]
        .as_array()
        .into_iter()
        .flatten()
        .filter_map(|g| g["key"].as_str().map(str::to_string))
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::fake::FakeCaller;
    use crate::mcp::McpError;
    use serde_json::{Value, json};
    use std::cell::RefCell;
    use std::collections::VecDeque;
    use std::rc::Rc;
    use std::time::Duration;

    fn list_reply() -> Value {
        json!({"result":[{"id":"103","protocol":"I2C","enabled":true,
            "channels":[{"role":"Data","channel":"D1"},{"role":"Clock","channel":"D0"}]}],
            "status":"Success"})
    }

    fn data_reply(keys: &[&str]) -> Value {
        let groups: Vec<Value> = keys.iter().map(|k| json!({"key":k,"count":1,"errorCount":0})).collect();
        json!({"result":{"id":"103","groupBy":"Data","groups":groups},"status":"Success"})
    }

    fn addr_reply(keys: &[&str]) -> Value {
        let groups: Vec<Value> = keys.iter().map(|k| json!({"key":k,"count":1})).collect();
        json!({"result":{"id":"103","groupBy":"Address","groups":groups},"status":"Success"})
    }

    /// Fake PicoScope: decoder list always OK; `Data` summaries come from
    /// the queue; `Address` summaries answer `addr`.
    fn scope(data: Vec<Result<Value, McpError>>, addr: Vec<&'static str>) -> (FakeCaller, Rc<RefCell<VecDeque<Result<Value, McpError>>>>) {
        let q = Rc::new(RefCell::new(VecDeque::from(data)));
        let q2 = q.clone();
        let c = FakeCaller::new(move |name, args| match name {
            "get_serial_decoder_list" => Ok(list_reply()),
            "get_serial_decoder_summary" if args["groupBy"] == "Address" => Ok(addr_reply(&addr)),
            "get_serial_decoder_summary" => q2.borrow_mut().pop_front().expect("no more data replies"),
            other => panic!("unexpected tool {other}"),
        });
        (c, q)
    }

    fn at(t0: Instant, s: f64) -> Instant {
        t0 + Duration::from_secs_f64(s)
    }

    #[test]
    fn poll_once_goes_live_with_value() {
        let (mut c, _) = scope(vec![Ok(data_reply(&["01 08 "]))], vec!["23 "]);
        let mut t = Tracker::new();
        let r = t.poll_once(&mut c, Instant::now());
        let s = t.snapshot();
        assert_eq!(s.conn, Conn::Live);
        assert_eq!(s.value, Some(0x08));
        assert_eq!(s.lit(), [false, false, false, true, false, false, false, false]);
        assert!(r.changed);
    }

    #[test]
    fn poll_once_same_value_not_changed() {
        let (mut c, _) = scope(vec![Ok(data_reply(&["01 08 "])), Ok(data_reply(&["01 08 "]))], vec![]);
        let mut t = Tracker::new();
        let t0 = Instant::now();
        t.poll_once(&mut c, t0);
        let r = t.poll_once(&mut c, at(t0, 0.05));
        assert!(!r.changed);
        assert_eq!(t.snapshot().conn, Conn::Live);
    }

    #[test]
    fn poll_once_counts_skip() {
        let (mut c, _) = scope(vec![Ok(data_reply(&["01 02 "])), Ok(data_reply(&["01 08 "]))], vec![]);
        let mut t = Tracker::new();
        let t0 = Instant::now();
        assert!(!t.poll_once(&mut c, t0).skipped);
        let r = t.poll_once(&mut c, at(t0, 0.1));
        assert!(r.changed && r.skipped);
    }

    #[test]
    fn next_step_is_not_a_skip() {
        let (mut c, _) = scope(vec![Ok(data_reply(&["01 80 "])), Ok(data_reply(&["01 01 "]))], vec![]);
        let mut t = Tracker::new();
        let t0 = Instant::now();
        t.poll_once(&mut c, t0);
        let r = t.poll_once(&mut c, at(t0, 0.1));
        assert!(r.changed && !r.skipped);
    }

    #[test]
    fn no_frames_after_two_seconds() {
        let (mut c, _) = scope(
            vec![Ok(data_reply(&["01 08 "])), Ok(data_reply(&["01 "])), Ok(data_reply(&["01 "]))],
            vec![],
        );
        let mut t = Tracker::new();
        let t0 = Instant::now();
        t.poll_once(&mut c, t0);
        t.poll_once(&mut c, at(t0, 1.0));
        assert_eq!(t.snapshot().conn, Conn::Live);
        t.poll_once(&mut c, at(t0, 2.1));
        let s = t.snapshot();
        assert_eq!(s.conn, Conn::NoFrames);
        assert_eq!(s.value, Some(0x08));
        assert!(s.lit()[3]);
        assert_eq!(s.status_text(), "no I2C frames");
    }

    #[test]
    fn frames_again_after_no_frames_goes_live() {
        let (mut c, _) = scope(
            vec![Ok(data_reply(&["01 08 "])), Ok(data_reply(&[])), Ok(data_reply(&["01 10 "]))],
            vec![],
        );
        let mut t = Tracker::new();
        let t0 = Instant::now();
        t.poll_once(&mut c, t0);
        t.poll_once(&mut c, at(t0, 2.5));
        assert_eq!(t.snapshot().conn, Conn::NoFrames);
        t.poll_once(&mut c, at(t0, 2.6));
        assert_eq!(t.snapshot().conn, Conn::Live);
        assert_eq!(t.snapshot().value, Some(0x10));
    }

    #[test]
    fn poll_once_error_goes_waiting() {
        let (mut c, _) = scope(
            vec![Ok(data_reply(&["01 08 "])), Err(McpError::Connect("refused".into()))],
            vec![],
        );
        let mut t = Tracker::new();
        let t0 = Instant::now();
        t.poll_once(&mut c, t0);
        t.poll_once(&mut c, at(t0, 0.05));
        let s = t.snapshot();
        assert_eq!(s.conn, Conn::Waiting);
        assert_eq!(s.lit(), [false; 8]);
        assert_eq!(s.status_text(), "waiting for PicoScope 7 …");
    }

    #[test]
    fn starts_waiting_before_first_poll() {
        let s = Tracker::new().snapshot();
        assert_eq!(s.conn, Conn::Waiting);
        assert_eq!(s.status_text(), "waiting for PicoScope 7 …");
    }

    #[test]
    fn slow_reply_does_not_change_state() {
        // Stalls are mirrored: the tracker has no notion of reply time, so a
        // poll 1.2 s after the previous one with a valid frame stays Live.
        let (mut c, _) = scope(vec![Ok(data_reply(&["01 08 "])), Ok(data_reply(&["01 10 "]))], vec![]);
        let mut t = Tracker::new();
        let t0 = Instant::now();
        t.poll_once(&mut c, t0);
        t.poll_once(&mut c, at(t0, 1.2));
        assert_eq!(t.snapshot().conn, Conn::Live);
        assert_eq!(t.snapshot().value, Some(0x10));
    }

    #[test]
    fn summary_rejected_reruns_setup() {
        let (mut c, _) = scope(
            vec![
                Ok(json!({"status":"Rejected","message":"Unknown decoder"})),
                Ok(data_reply(&["01 08 "])),
            ],
            vec![],
        );
        let mut t = Tracker::new();
        let t0 = Instant::now();
        t.poll_once(&mut c, t0);
        assert_eq!(c.count("get_serial_decoder_list"), 1);
        t.poll_once(&mut c, at(t0, 0.05));
        assert_eq!(c.count("get_serial_decoder_list"), 2);
        assert_eq!(t.snapshot().value, Some(0x08));
    }

    #[test]
    fn address_queried_once_per_second() {
        let (mut c, _) = scope(
            vec![Ok(data_reply(&["01 08 "])), Ok(data_reply(&["01 08 "])), Ok(data_reply(&["01 10 "]))],
            vec!["23 "],
        );
        let mut t = Tracker::new();
        let t0 = Instant::now();
        t.poll_once(&mut c, t0);
        t.poll_once(&mut c, at(t0, 0.05));
        t.poll_once(&mut c, at(t0, 1.05));
        let addr_calls = c.calls.iter().filter(|(_, a)| a["groupBy"] == "Address").count();
        assert_eq!(addr_calls, 2);
        assert_eq!(t.snapshot().status_text(), "last frame: addr 0x23 · reg 0x01 · data 0x10");
    }

    #[test]
    fn unknown_address_is_question_mark() {
        let (mut c, _) = scope(vec![Ok(data_reply(&["01 08 "]))], vec![]);
        let mut t = Tracker::new();
        t.poll_once(&mut c, Instant::now());
        assert_eq!(t.snapshot().status_text(), "last frame: addr ? · reg 0x01 · data 0x08");
    }

    #[test]
    fn live_without_value_waits_for_frames() {
        let (mut c, _) = scope(vec![Ok(data_reply(&[]))], vec![]);
        let mut t = Tracker::new();
        t.poll_once(&mut c, Instant::now());
        assert_eq!(t.snapshot().conn, Conn::Live);
        assert_eq!(t.snapshot().status_text(), "waiting for I2C frames …");
    }
}
