// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
//! Talker listening (audio_pipe live): raw 16 kHz mono S16_LE PCM into a player child process
//! (`aplay` by default, `PDM_PLAYER` to override). Pushing never blocks: the
//! samples go through a bounded channel to a writer thread, and a full
//! channel drops the chunk (a stalled player must not stall the window).

use std::io::Write;
use std::process::{Child, Command, Stdio};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::{sync_channel, SyncSender};
use std::sync::{Arc, Mutex};

/// The player command: `PDM_PLAYER` split on whitespace, else `aplay` with a
/// 0.1 s buffer (raw 16 kHz mono S16_LE on stdin).
pub fn player_cmd() -> Vec<String> {
    match std::env::var("PDM_PLAYER") {
        Ok(v) if !v.trim().is_empty() => v.split_whitespace().map(str::to_string).collect(),
        _ => "aplay -q -t raw -f S16_LE -r 16000 -c 1 --buffer-time=100000".split(' ').map(str::to_string).collect(),
    }
}

pub struct Player {
    tx: Option<SyncSender<Vec<i16>>>,
    child: Arc<Mutex<Child>>,
    ok: Arc<AtomicBool>,
    writer: Option<std::thread::JoinHandle<()>>,
}

impl Player {
    /// Starts `cmd` with a piped stdin.
    pub fn spawn(cmd: &[String]) -> std::io::Result<Player> {
        let (bin, args) = cmd.split_first().ok_or_else(|| std::io::Error::other("empty player command"))?;
        let mut child = Command::new(bin).args(args).stdin(Stdio::piped()).stdout(Stdio::null()).stderr(Stdio::null()).spawn()?;
        let mut stdin = child.stdin.take().ok_or_else(|| std::io::Error::other("no stdin"))?;
        let (tx, rx) = sync_channel::<Vec<i16>>(32);
        let ok = Arc::new(AtomicBool::new(true));
        let ok2 = ok.clone();
        let writer = std::thread::spawn(move || {
            for x in rx {
                let b: Vec<u8> = x.iter().flat_map(|v| v.to_le_bytes()).collect();
                if stdin.write_all(&b).is_err() {
                    break;
                }
            }
            ok2.store(false, Ordering::Relaxed);
        });
        Ok(Player { tx: Some(tx), child: Arc::new(Mutex::new(child)), ok, writer: Some(writer) })
    }

    /// Queues samples; drops them when the player is behind (never blocks).
    pub fn push(&self, pcm: &[i16]) {
        if let Some(tx) = &self.tx {
            let _ = tx.try_send(pcm.to_vec());
        }
    }

    /// False once the player process or its writer has ended.
    pub fn alive(&self) -> bool {
        self.ok.load(Ordering::Relaxed) && self.child.lock().map(|mut c| matches!(c.try_wait(), Ok(None))).unwrap_or(false)
    }
}

impl Drop for Player {
    fn drop(&mut self) {
        use std::time::{Duration, Instant};
        drop(self.tx.take()); // the writer sends what is queued, then closes stdin
        let t0 = Instant::now();
        let done = |w: &Option<std::thread::JoinHandle<()>>| w.as_ref().is_none_or(|w| w.is_finished());
        while !done(&self.writer) && t0.elapsed() < Duration::from_millis(100) {
            std::thread::sleep(Duration::from_millis(2));
        }
        if let Ok(mut c) = self.child.lock() {
            // a player that is still busy after its input ended (or stalled) is stopped
            while matches!(c.try_wait(), Ok(None)) && t0.elapsed() < Duration::from_millis(200) {
                std::thread::sleep(Duration::from_millis(2));
            }
            let _ = c.kill();
            let _ = c.wait();
        }
        // the writer is not joined: a grandchild may still hold the pipe; its
        // write fails (and the thread ends) once the pipe closes
        drop(self.writer.take());
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::{Duration, Instant};

    fn sh(script: &str) -> Vec<String> {
        vec!["/bin/sh".into(), "-c".into(), script.into()]
    }

    #[test]
    fn player_writes_pcm() {
        let d = tempfile::tempdir().unwrap();
        let f = d.path().join("out.raw");
        let p = Player::spawn(&sh(&format!("cat > {}", f.display()))).unwrap();
        let x: Vec<i16> = (0..1000).map(|i| (i * 7 - 3000) as i16).collect();
        p.push(&x);
        drop(p);
        let b = std::fs::read(&f).unwrap();
        assert_eq!(b.len(), 2000);
        let back: Vec<i16> = b.chunks(2).map(|c| i16::from_le_bytes([c[0], c[1]])).collect();
        assert_eq!(back, x);
    }

    #[test]
    fn full_player_drops_not_blocks() {
        let p = Player::spawn(&sh("sleep 5")).unwrap();
        let x = vec![0i16; 16000];
        let t0 = Instant::now();
        for _ in 0..200 {
            p.push(&x);
        }
        assert!(t0.elapsed() < Duration::from_millis(200), "{:?}", t0.elapsed());
    }

    #[test]
    fn dropping_a_stalled_player_is_quick() {
        // switching the listening source drops the player on the UI thread
        let p = Player::spawn(&sh("sleep 5")).unwrap();
        for _ in 0..40 {
            p.push(&[0; 16000]); // fills the pipe: the writer blocks
        }
        std::thread::sleep(Duration::from_millis(50));
        let t0 = Instant::now();
        drop(p);
        assert!(t0.elapsed() < Duration::from_millis(500), "{:?}", t0.elapsed());
    }

    #[test]
    fn missing_player_is_an_error() {
        assert!(Player::spawn(&["/no/such/player".to_string()]).is_err());
    }

    #[test]
    fn dead_player_is_not_alive() {
        let p = Player::spawn(&sh("exit 0")).unwrap();
        let t0 = Instant::now();
        while p.alive() && t0.elapsed() < Duration::from_secs(2) {
            p.push(&[0; 160]);
            std::thread::sleep(Duration::from_millis(10));
        }
        assert!(!p.alive());
    }
}
