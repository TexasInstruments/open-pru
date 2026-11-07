// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! Text lanes under the spectrograms (booth transcripts design 4, layout B):
//! one lane per talker, found by azimuth, each sentence placed at the board
//! index where it was spoken.

pub use crate::transcribe::TextLine;
use eframe::egui::Color32;

pub const MAX_LANES: usize = 6;
/// A talker within this many degrees of a lane's azimuth uses that lane.
pub const REUSE_DEG: f32 = 20.0;
/// A lane without new text for this long (10 s at 16 kHz) fades out.
pub const FADE_FRAMES: u64 = 160_000;

#[derive(Clone, Debug)]
pub struct Lane {
    /// azimuth of its newest text
    pub az: f32,
    /// palette index, kept for the lane's lifetime
    pub colour: usize,
    /// lane number shown, 1-based
    pub label: usize,
    pub items: Vec<TextLine>,
    /// board index of the newest text's end of speech
    pub last_end: u64,
}

#[derive(Clone, Debug, Default)]
pub struct Lanes {
    /// in label order
    pub lanes: Vec<Lane>,
}

fn ang_dist(a: f32, b: f32) -> f32 {
    let d = (a - b).rem_euclid(360.0);
    d.min(360.0 - d)
}

impl Lanes {
    /// Put `t` in the closest lane within REUSE_DEG, else a new lane; with
    /// MAX_LANES lanes the least recently used one gives way.
    pub fn add(&mut self, t: TextLine) {
        let near = self
            .lanes
            .iter()
            .enumerate()
            .map(|(i, l)| (i, ang_dist(l.az, t.az)))
            .filter(|(_, d)| *d <= REUSE_DEG)
            .min_by(|a, b| a.1.total_cmp(&b.1))
            .map(|(i, _)| i);
        let i = match near {
            Some(i) => i,
            None => {
                if self.lanes.len() >= MAX_LANES {
                    let old = (0..self.lanes.len()).min_by_key(|&i| self.lanes[i].last_end).unwrap();
                    self.lanes.remove(old);
                }
                let label = (1..=MAX_LANES).find(|n| self.lanes.iter().all(|l| l.label != *n)).unwrap();
                self.lanes.push(Lane { az: t.az, colour: label - 1, label, items: Vec::new(), last_end: 0 });
                self.lanes.sort_by_key(|l| l.label);
                self.lanes.iter().position(|l| l.label == label).unwrap()
            }
        };
        let l = &mut self.lanes[i];
        l.az = t.az;
        l.last_end = l.last_end.max(t.end);
        l.items.push(t);
    }

    /// Drop lanes without text for FADE_FRAMES and items that ended more than 10 s ago.
    pub fn prune(&mut self, now_index: u64) {
        self.lanes.retain(|l| now_index.saturating_sub(l.last_end) <= FADE_FRAMES);
        for l in &mut self.lanes {
            l.items.retain(|t| now_index.saturating_sub(t.end) <= FADE_FRAMES);
        }
    }
}

/// Lane colours: six hues that read on the dark background.
pub fn palette(i: usize) -> Color32 {
    const C: [[u8; 3]; MAX_LANES] = [
        [74, 163, 255],  // blue
        [255, 111, 168], // pink
        [155, 210, 74],  // green
        [255, 184, 74],  // amber
        [178, 132, 255], // violet
        [72, 214, 200],  // teal
    ];
    let [r, g, b] = C[i % MAX_LANES];
    Color32::from_rgb(r, g, b)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn t(az: f32, end: u64) -> TextLine {
        TextLine { pass: 1, scene: "a".into(), speaker: 0, az, start: end.saturating_sub(16000), end, text: "x".into(), latency_ms: 500.0 }
    }

    #[test]
    fn reuses_lane_within_20_degrees() {
        let mut l = Lanes::default();
        l.add(t(221.8, 100));
        l.add(t(214.0, 200_000));
        assert_eq!(l.lanes.len(), 1);
        assert_eq!(l.lanes[0].items.len(), 2);
        l.add(t(252.0, 200_100));
        assert_eq!(l.lanes.len(), 2);
        assert_eq!((l.lanes[0].label, l.lanes[1].label), (1, 2));
        assert_ne!(l.lanes[0].colour, l.lanes[1].colour);
    }

    #[test]
    fn wraps_around_360() {
        let mut l = Lanes::default();
        l.add(t(355.0, 100));
        l.add(t(5.0, 200));
        assert_eq!(l.lanes.len(), 1);
    }

    #[test]
    fn fades_after_10_s() {
        let mut l = Lanes::default();
        l.add(t(30.0, 0));
        l.prune(160_000);
        assert_eq!(l.lanes.len(), 1);
        l.prune(160_001);
        assert_eq!(l.lanes.len(), 0);
    }

    #[test]
    fn old_items_are_dropped() {
        let mut l = Lanes::default();
        l.add(t(30.0, 16_000));
        l.add(t(31.0, 200_000));
        l.prune(200_000);
        assert_eq!(l.lanes[0].items.len(), 1);
        assert_eq!(l.lanes[0].items[0].end, 200_000);
    }

    #[test]
    fn seventh_talker_replaces_least_recent() {
        let mut l = Lanes::default();
        for (i, az) in [0.0, 50.0, 100.0, 150.0, 200.0, 250.0].into_iter().enumerate() {
            l.add(t(az, 1000 + 10 * i as u64));
        }
        assert_eq!(l.lanes.len(), MAX_LANES);
        l.add(t(300.0, 2000));
        assert_eq!(l.lanes.len(), MAX_LANES);
        assert!(l.lanes.iter().all(|x| (x.az - 0.0).abs() > 1.0), "the oldest lane (0°) is gone");
        assert!(l.lanes.iter().any(|x| (x.az - 300.0).abs() < 1.0));
        let mut labels: Vec<usize> = l.lanes.iter().map(|x| x.label).collect();
        labels.sort();
        assert_eq!(labels, vec![1, 2, 3, 4, 5, 6]);
    }

    #[test]
    fn palette_has_six_distinct_colours() {
        let c: Vec<_> = (0..MAX_LANES).map(palette).collect();
        for i in 0..c.len() {
            for j in 0..i {
                assert_ne!(c[i], c[j]);
            }
        }
    }
}
