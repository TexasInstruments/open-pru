// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! Command-line checks that need no window and no board.

#[test]
fn transcribe_needs_playlist() {
    let out = std::process::Command::new(env!("CARGO_BIN_EXE_pdm-eth"))
        .args(["gui", "--emulator", "--transcribe", "/bin/true"])
        .output()
        .unwrap();
    assert!(!out.status.success());
    let err = String::from_utf8_lossy(&out.stderr);
    assert!(err.contains("--transcribe needs --playlist"), "{err}");
}

#[test]
fn scene_and_playlist_conflict() {
    let out = std::process::Command::new(env!("CARGO_BIN_EXE_pdm-eth"))
        .args(["gui", "--emulator", "--scene", "a.zip", "--playlist", "b"])
        .output()
        .unwrap();
    assert!(!out.status.success());
    let err = String::from_utf8_lossy(&out.stderr);
    assert!(err.contains("--scene") && err.contains("cannot be used with"), "{err}");
}

#[test]
fn missing_scene_is_an_error() {
    let out = std::process::Command::new(env!("CARGO_BIN_EXE_pdm-eth"))
        .args(["gui", "--emulator", "--scene", "/no/such/scene.zip"])
        .output()
        .unwrap();
    assert!(!out.status.success());
    let err = String::from_utf8_lossy(&out.stderr);
    assert!(err.contains("/no/such/scene.zip"), "{err}");
}

#[test]
fn listen_takes_a_channel() {
    let out = std::process::Command::new(env!("CARGO_BIN_EXE_pdm-eth"))
        .args(["gui", "--emulator", "--listen", "ch9"])
        .output()
        .unwrap();
    assert!(!out.status.success());
    let err = String::from_utf8_lossy(&out.stderr);
    assert!(err.contains("ch0 to ch7"), "{err}");
}
