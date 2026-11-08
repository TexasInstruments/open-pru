// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! Supervises `audio_pipe live` (booth transcripts design 4): one control
//! line per pass start to its stdin, JSON text lines from its stdout, and
//! the restart policy (at most one start per 10 s, off after 3 quick
//! failures in a row). The window keeps working whatever the child does.

use std::io::{BufRead, Write};
use std::process::{Child, ChildStdin, Command, Stdio};
use std::sync::mpsc::{Receiver, RecvTimeoutError};
use std::time::{Duration, Instant};

/// At most one start per this long.
pub const RESTART_EVERY: Duration = Duration::from_secs(10);
/// A child that exits sooner than this after its start failed to start.
pub const QUICK_EXIT: Duration = Duration::from_secs(5);
/// Failed starts in a row before giving up.
pub const MAX_FAILS: u32 = 3;
/// A child that ran this long resets the failure count.
pub const GOOD_RUN: Duration = Duration::from_secs(60);

/// One final utterance from audio_pipe; `start`/`end` are board indices.
#[derive(Clone, Debug, PartialEq, serde::Deserialize, serde::Serialize)]
pub struct TextLine {
    pub pass: u64,
    pub scene: String,
    pub speaker: usize,
    pub az: f32,
    pub start: u64,
    pub end: u64,
    pub text: String,
    pub latency_ms: f32,
}

#[derive(Clone, Debug, PartialEq)]
pub enum TStatus {
    /// no more starts; the reason
    Off(String),
    /// not started yet, or restarting at the next pass start
    Starting,
    /// running, waiting for a pass start
    Waiting,
    Running,
    /// fell behind; joins again at the next pass start
    Overrun,
}

impl std::fmt::Display for TStatus {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            TStatus::Off(r) => write!(f, "off: {r}"),
            TStatus::Starting => f.write_str("starting"),
            TStatus::Waiting => f.write_str("waiting for audio"),
            TStatus::Running => f.write_str("running"),
            TStatus::Overrun => f.write_str("fell behind, rejoining"),
        }
    }
}

pub enum Parsed {
    Text(TextLine),
    Status(TStatus),
}

/// A line of audio_pipe's stdout; anything else than a text or status line is None.
pub fn parse_line(s: &str) -> Option<Parsed> {
    let s = s.trim();
    if let Ok(t) = serde_json::from_str::<TextLine>(s) {
        return Some(Parsed::Text(t));
    }
    let v: serde_json::Value = serde_json::from_str(s).ok()?;
    let st = match v.get("status")?.as_str()? {
        "waiting" => TStatus::Waiting,
        "running" => TStatus::Running,
        "overrun" => TStatus::Overrun,
        _ => return None,
    };
    Some(Parsed::Status(st))
}

struct Run {
    child: Child,
    stdin: Option<ChildStdin>,
    rx: Receiver<Parsed>,
    started: Instant,
}

pub struct TextPipe {
    cmd: Vec<String>,
    run: Option<Run>,
    last_start: Option<Instant>,
    fails: u32,
    status: TStatus,
    /// Talker listening: the azimuth audio_pipe plays (sent again after a restart).
    listen_az: Option<f32>,
}

impl TextPipe {
    /// `cmd`: argv of `audio_pipe live --shm NAME ...`; started at the first pass start.
    pub fn new(cmd: Vec<String>) -> Self {
        TextPipe { cmd, run: None, last_start: None, fails: 0, status: TStatus::Starting, listen_az: None }
    }

    pub fn status(&self) -> &TStatus {
        &self.status
    }

    /// A child process is running.
    pub fn running(&self) -> bool {
        self.run.is_some()
    }

    fn start(&mut self, now: Instant) {
        self.last_start = Some(now);
        let Some((bin, args)) = self.cmd.split_first() else {
            self.status = TStatus::Off("no command".into());
            return;
        };
        let spawned = Command::new(bin).args(args).stdin(Stdio::piped()).stdout(Stdio::piped()).stderr(Stdio::inherit()).spawn();
        let mut child = match spawned {
            Ok(c) => c,
            Err(e) if e.kind() == std::io::ErrorKind::NotFound => {
                self.status = TStatus::Off(format!("audio_pipe not found: {bin}"));
                return;
            }
            Err(e) => {
                self.failed(format!("{bin}: {e}"));
                return;
            }
        };
        let out = child.stdout.take().unwrap();
        let (tx, rx) = std::sync::mpsc::channel();
        std::thread::spawn(move || {
            for line in std::io::BufReader::new(out).lines() {
                let Ok(line) = line else { return };
                if let Some(p) = parse_line(&line)
                    && tx.send(p).is_err() {
                        return;
                    }
            }
        });
        let stdin = child.stdin.take();
        self.run = Some(Run { child, stdin, rx, started: now });
        self.status = TStatus::Starting;
        if self.listen_az.is_some() {
            self.send(serde_json::json!({"listen_az": self.listen_az}));
        }
    }

    /// One control line to the child (a failed write: the child is gone; poll notices).
    fn send(&mut self, line: serde_json::Value) {
        if let Some(r) = &mut self.run
            && let Some(w) = &mut r.stdin
            && writeln!(w, "{line}").and_then(|_| w.flush()).is_err() {
                r.stdin = None;
            }
    }

    /// Talker listening: audio_pipe plays the stream nearest `az`, or stops (None).
    pub fn listen(&mut self, az: Option<f32>) {
        self.listen_az = az;
        self.send(serde_json::json!({"listen_az": az}));
    }

    pub fn listening(&self) -> Option<f32> {
        self.listen_az
    }

    fn failed(&mut self, reason: String) {
        self.fails += 1;
        self.status = if self.fails >= MAX_FAILS {
            TStatus::Off(format!("{reason} ({} failed starts)", self.fails))
        } else {
            TStatus::Starting
        };
    }

    /// A pass starts at board index `start`: start (or restart) the child if
    /// the policy allows, then send it the control line.
    pub fn pass_start(&mut self, now: Instant, pass: u64, scene: &str, start: u64, scale: f32) {
        if matches!(self.status, TStatus::Off(_)) {
            return;
        }
        if self.run.is_none() {
            if self.last_start.is_some_and(|t| now.saturating_duration_since(t) < RESTART_EVERY) {
                return;
            }
            self.start(now);
        }
        self.send(serde_json::json!({"pass": pass, "scene": scene, "start": start, "scale": scale}));
    }

    /// New text lines; notices a child that exited (non-blocking).
    pub fn poll(&mut self, now: Instant) -> Vec<TextLine> {
        let mut v = Vec::new();
        let Some(r) = &mut self.run else { return v };
        let mut take = |p: Parsed, status: &mut TStatus| match p {
            Parsed::Text(t) => v.push(t),
            Parsed::Status(s) => *status = s,
        };
        while let Ok(p) = r.rx.try_recv() {
            take(p, &mut self.status);
        }
        match r.child.try_wait() {
            Ok(None) => {}
            Ok(Some(_)) | Err(_) => {
                let code = r.child.try_wait().ok().flatten().map_or("?".to_string(), |c| c.to_string());
                // the lines printed before the exit
                let t0 = Instant::now();
                loop {
                    match r.rx.recv_timeout(Duration::from_millis(50)) {
                        Ok(p) => take(p, &mut self.status),
                        Err(RecvTimeoutError::Disconnected) => break,
                        Err(RecvTimeoutError::Timeout) if t0.elapsed() > Duration::from_millis(500) => break,
                        Err(RecvTimeoutError::Timeout) => {}
                    }
                }
                let ran = now.saturating_duration_since(r.started);
                self.run = None;
                if ran >= GOOD_RUN {
                    self.fails = 0;
                }
                if ran < QUICK_EXIT {
                    self.failed(format!("audio_pipe exited ({code})"));
                } else {
                    self.status = TStatus::Starting;
                }
                eprintln!("transcription: audio_pipe exited ({code}) after {:.0} s, {}", ran.as_secs_f64(), self.status);
            }
        }
        v
    }
}

impl Drop for TextPipe {
    fn drop(&mut self) {
        if let Some(mut r) = self.run.take() {
            let _ = r.child.kill();
            let _ = r.child.wait();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const TEXT: &str = r#"{"pass":12,"scene":"a","speaker":3,"az":221.8,"start":145343168,"end":145395200,"text":"Please bring the red toolbox to station 4.","latency_ms":512.0}"#;

    /// argv running a shell script written to `dir` (through /bin/sh: executing a file
    /// just written races with forks in parallel tests, ETXTBSY).
    fn script(dir: &std::path::Path, name: &str, body: &str) -> Vec<String> {
        let p = dir.join(name);
        std::fs::write(&p, format!("{body}\n")).unwrap();
        vec!["/bin/sh".into(), p.display().to_string()]
    }

    /// Polls (with the fake time `now`) until `done` or 3 s of real time; returns the text lines.
    fn poll_until(tp: &mut TextPipe, now: Instant, done: impl Fn(&TextPipe, usize) -> bool) -> Vec<TextLine> {
        let t0 = Instant::now();
        let mut v = Vec::new();
        while t0.elapsed() < Duration::from_secs(3) {
            v.extend(tp.poll(now));
            if done(tp, v.len()) {
                break;
            }
            std::thread::sleep(Duration::from_millis(10));
        }
        v
    }

    #[test]
    fn parses_text_and_status_lines() {
        match parse_line(TEXT) {
            Some(Parsed::Text(t)) => {
                assert_eq!((t.pass, t.speaker, t.start, t.end), (12, 3, 145343168, 145395200));
                assert_eq!(t.text, "Please bring the red toolbox to station 4.");
                assert!((t.az - 221.8).abs() < 1e-3 && (t.latency_ms - 512.0).abs() < 1e-3);
            }
            _ => panic!("not a text line"),
        }
        assert!(matches!(parse_line(r#"{"status":"waiting","detail":""}"#), Some(Parsed::Status(TStatus::Waiting))));
        assert!(matches!(parse_line(r#"{"status":"running","detail":""}"#), Some(Parsed::Status(TStatus::Running))));
        assert!(matches!(parse_line(r#"{"status":"overrun","detail":"x"}"#), Some(Parsed::Status(TStatus::Overrun))));
    }

    #[test]
    fn ignores_garbage() {
        for g in ["whisper_init: loading model", "{", "", r#"{"pass":"x"}"#] {
            assert!(parse_line(g).is_none(), "{g:?}");
        }
        let d = tempfile::tempdir().unwrap();
        let body = format!("echo '{TEXT}'\necho 'whisper_init: loading model'\necho '{{'\necho ''\necho '{{\"pass\":\"x\"}}'\necho '{TEXT}'\nsleep 2");
        let mut tp = TextPipe::new(script(d.path(), "c", &body));
        let now = Instant::now();
        tp.pass_start(now, 1, "a", 0, 1.0);
        let v = poll_until(&mut tp, now, |_, n| n >= 2);
        std::thread::sleep(Duration::from_millis(200));
        let more = tp.poll(now);
        assert_eq!(v.len() + more.len(), 2);
    }

    #[test]
    fn listen_is_sent_and_resent_after_restart() {
        let d = tempfile::tempdir().unwrap();
        let ctl = d.path().join("ctl");
        // copies its stdin lines for 0.5 s, then exits
        let body = format!("timeout 0.5 cat >> {}\nexit 0", ctl.display());
        let mut tp = TextPipe::new(script(d.path(), "c", &body));
        let t0 = Instant::now();
        tp.pass_start(t0, 1, "a", 1000, 0.5);
        tp.listen(Some(211.0));
        poll_until(&mut tp, t0, |tp, _| !tp.running());
        tp.pass_start(t0 + Duration::from_secs(11), 2, "a", 2000, 0.5); // restarted: the choice is sent again
        poll_until(&mut tp, t0 + Duration::from_secs(11), |tp, _| !tp.running());
        let c = std::fs::read_to_string(&ctl).unwrap();
        assert_eq!(c.matches(r#""listen_az":211"#).count(), 2, "{c}");
        tp.listen(None);
        assert_eq!(tp.listening(), None);
    }

    #[test]
    fn restarts_after_exit() {
        let d = tempfile::tempdir().unwrap();
        let ctl = d.path().join("ctl");
        let body = format!("read l\necho \"$l\" >> {}\necho '{TEXT}'\nexit 0", ctl.display());
        let mut tp = TextPipe::new(script(d.path(), "c", &body));
        let t0 = Instant::now();
        tp.pass_start(t0, 1, "a", 1000, 0.5);
        let v = poll_until(&mut tp, t0, |tp, _| !tp.running());
        assert_eq!(v.len(), 1, "the line printed before the exit");
        assert_eq!(*tp.status(), TStatus::Starting);
        tp.pass_start(t0 + Duration::from_secs(5), 2, "b", 2000, 0.5); // < 10 s after the start: not yet
        assert!(!tp.running());
        tp.pass_start(t0 + Duration::from_secs(11), 3, "c", 3000, 0.5);
        assert!(tp.running());
        poll_until(&mut tp, t0 + Duration::from_secs(11), |tp, _| !tp.running());
        let lines: Vec<serde_json::Value> = std::fs::read_to_string(&ctl).unwrap().lines().map(|l| serde_json::from_str(l).unwrap()).collect();
        assert_eq!(lines.len(), 2);
        assert_eq!((lines[0]["pass"].as_u64(), lines[0]["start"].as_u64(), lines[0]["scale"].as_f64()), (Some(1), Some(1000), Some(0.5)));
        assert_eq!((lines[1]["pass"].as_u64(), lines[1]["scene"].as_str()), (Some(3), Some("c")));
    }

    #[test]
    fn gives_up_after_three_quick_failures() {
        let d = tempfile::tempdir().unwrap();
        let starts = d.path().join("starts");
        let mut tp = TextPipe::new(script(d.path(), "c", &format!("echo x >> {}\nexit 1", starts.display())));
        let t0 = Instant::now();
        for i in 0..4 {
            let now = t0 + Duration::from_secs(11 * i);
            tp.pass_start(now, i, "a", 0, 1.0);
            poll_until(&mut tp, now, |tp, _| !tp.running());
        }
        assert_eq!(std::fs::read_to_string(&starts).unwrap().lines().count(), 3);
        assert!(matches!(tp.status(), TStatus::Off(_)), "{:?}", tp.status());
    }

    #[test]
    fn long_run_resets_the_failure_count() {
        let d = tempfile::tempdir().unwrap();
        let starts = d.path().join("starts");
        let mut tp = TextPipe::new(script(d.path(), "c", &format!("echo x >> {}\nexit 1", starts.display())));
        let t0 = Instant::now();
        for i in 0..2 {
            let now = t0 + Duration::from_secs(11 * i);
            tp.pass_start(now, i, "a", 0, 1.0);
            poll_until(&mut tp, now, |tp, _| !tp.running());
        }
        // a run that lasted 61 s
        let now = t0 + Duration::from_secs(30);
        tp.pass_start(now, 2, "a", 0, 1.0);
        poll_until(&mut tp, now + Duration::from_secs(61), |tp, _| !tp.running());
        for i in 0..2 {
            let now = t0 + Duration::from_secs(100 + 11 * i);
            tp.pass_start(now, 3 + i, "a", 0, 1.0);
            poll_until(&mut tp, now, |tp, _| !tp.running());
        }
        assert_eq!(*tp.status(), TStatus::Starting);
        assert_eq!(std::fs::read_to_string(&starts).unwrap().lines().count(), 5);
    }

    #[test]
    fn missing_binary_is_off() {
        let mut tp = TextPipe::new(vec!["/nonexistent".into()]);
        tp.pass_start(Instant::now(), 1, "a", 0, 1.0);
        match tp.status() {
            TStatus::Off(r) => assert!(r.contains("not found") && r.contains("/nonexistent"), "{r}"),
            s => panic!("{s:?}"),
        }
    }
}
