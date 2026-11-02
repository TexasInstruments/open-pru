// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! The window: TCA9538 board with 8 LEDs and a status line (design.md, Look).

use crate::tracker::Snapshot;
use eframe::egui::{
    self, Align2, Color32, CornerRadius, FontId, Key, Pos2, Rect, Sense, Stroke, StrokeKind, ViewportCommand,
    pos2, vec2,
};
use std::sync::{Arc, Mutex};

const BG: Color32 = Color32::from_rgb(0x18, 0x18, 0x18);
const PCB: Color32 = Color32::from_rgb(0x0b, 0x4d, 0x2c);
const PCB_EDGE: Color32 = Color32::from_rgb(0x0e, 0x6b, 0x3c);
const CHIP: Color32 = Color32::from_rgb(0x22, 0x22, 0x22);
const CHIP_TEXT: Color32 = Color32::from_rgb(0xbb, 0xbb, 0xbb);
const LED_ON: Color32 = Color32::from_rgb(0xff, 0x3b, 0x3b);
const LED_OFF: Color32 = Color32::from_rgb(0x3a, 0x10, 0x10);
const LED_OFF_EDGE: Color32 = Color32::from_rgb(0x55, 0x22, 0x22);
const LABEL: Color32 = Color32::from_rgb(0xb8, 0xd8, 0xc4);
const STATUS: Color32 = Color32::from_rgb(0xcc, 0xcc, 0xcc);

/// Board width : height.
const BOARD_ASPECT: f32 = 2.4;

/// Centers of LEDs P0..P7, left to right, evenly spaced across `row`.
pub fn led_centers(row: Rect) -> [Pos2; 8] {
    let step = row.width() / 8.0;
    std::array::from_fn(|i| pos2(row.left() + step * (i as f32 + 0.5), row.center().y))
}

pub struct VizApp {
    shared: Arc<Mutex<Snapshot>>,
}

impl VizApp {
    pub fn new(shared: Arc<Mutex<Snapshot>>) -> Self {
        VizApp { shared }
    }
}

impl eframe::App for VizApp {
    fn ui(&mut self, ui: &mut egui::Ui, _frame: &mut eframe::Frame) {
        let ctx = ui.ctx().clone();
        if ctx.input(|i| i.key_pressed(Key::F11)) {
            let full = ctx.input(|i| i.viewport().fullscreen.unwrap_or(false));
            ctx.send_viewport_cmd(ViewportCommand::Fullscreen(!full));
        }
        let snap = self.shared.lock().unwrap().clone();
        let (response, painter) = ui.allocate_painter(ui.available_size(), Sense::hover());
        let area = response.rect;
        painter.rect_filled(area, CornerRadius::ZERO, BG);

        // Board, centered, with room for the status line below.
        let status_h = (area.height() * 0.12).clamp(18.0, 60.0);
        let free = Rect::from_min_max(area.min, pos2(area.max.x, area.max.y - status_h)).shrink(area.width().min(area.height()) * 0.04);
        let bw = free.width().min(free.height() * BOARD_ASPECT);
        let board = Rect::from_center_size(free.center(), vec2(bw, bw / BOARD_ASPECT));
        let bh = board.height();
        painter.rect(board, CornerRadius::same((bh * 0.05) as u8), PCB, Stroke::new(bh * 0.015, PCB_EDGE), StrokeKind::Inside);

        // Chip with the version tooltip.
        let chip = Rect::from_center_size(pos2(board.center().x, board.top() + bh * 0.2), vec2(bw * 0.36, bh * 0.17));
        painter.rect_filled(chip, CornerRadius::same((bh * 0.015) as u8), CHIP);
        painter.text(chip.center(), Align2::CENTER_CENTER, "TCA9538 @0x23", FontId::monospace(bh * 0.07), CHIP_TEXT);
        ui.interact(chip, ui.scope_id().with("chip"), Sense::hover())
            .on_hover_text(format!("I2C LED visualizer v{}", env!("CARGO_PKG_VERSION")));

        // LEDs P0..P7.
        let row = Rect::from_center_size(pos2(board.center().x, board.top() + bh * 0.56), vec2(bw * 0.9, bh * 0.25));
        let radius = (row.width() / 8.0 * 0.3).min(row.height() * 0.45);
        for (i, (c, on)) in led_centers(row).into_iter().zip(snap.lit()).enumerate() {
            if on {
                for k in 1..=4 {
                    let a = 70 - k * 15;
                    painter.circle_filled(c, radius * (1.0 + 0.25 * k as f32), Color32::from_rgba_unmultiplied(0xff, 0x3b, 0x3b, a as u8));
                }
                painter.circle(c, radius, LED_ON, Stroke::new(radius * 0.08, Color32::from_rgb(0xff, 0x88, 0x88)));
            } else {
                painter.circle(c, radius, LED_OFF, Stroke::new(radius * 0.08, LED_OFF_EDGE));
            }
            painter.text(pos2(c.x, c.y + radius * 1.9), Align2::CENTER_CENTER, format!("P{i}"), FontId::monospace(bh * 0.06), LABEL);
        }

        // Status line.
        painter.text(
            pos2(area.center().x, area.bottom() - status_h * 0.6),
            Align2::CENTER_CENTER,
            snap.status_text(),
            FontId::monospace(status_h * 0.45),
            STATUS,
        );
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use eframe::egui::{pos2, vec2};

    #[test]
    fn board_layout_scales() {
        let r = Rect::from_min_size(pos2(10.0, 20.0), vec2(400.0, 50.0));
        let c = led_centers(r);
        for w in c.windows(2) {
            assert!(w[1].x > w[0].x, "P0 leftmost, x strictly increasing");
        }
        assert!(c.iter().all(|p| r.contains(*p)));
        let step = c[1].x - c[0].x;
        assert!(c.windows(2).all(|w| ((w[1].x - w[0].x) - step).abs() < 1e-3), "even spacing");

        let wide = Rect::from_min_size(pos2(10.0, 20.0), vec2(800.0, 50.0));
        let cw = led_centers(wide);
        assert!(((cw[1].x - cw[0].x) - 2.0 * step).abs() < 1e-3, "spacing doubles");
    }
}
