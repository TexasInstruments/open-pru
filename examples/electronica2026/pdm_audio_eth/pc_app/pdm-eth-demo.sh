#!/usr/bin/env bash
# Start the pdm_audio_eth live window in one step, from any directory.
#
# Does what docs/demo_howto.md sections 2.1, 2.2, 6 and 7.1 do by hand:
#   1. cargo build --release (a no-op when nothing changed)
#   2. setcap cap_net_raw,cap_net_admin on the binary if a build removed it
#   3. take the NIC away from NetworkManager and bring it up
#   4. pdm-eth gui --iface <NIC> [your extra arguments]
#
# Booth transcripts: if the playlist folder exists (PDM_PLAYLIST, default
# pc_app/audio/booth, made by pctools/audio_pipe/tools/make_booth_scenes.sh)
# and no --chN/--set/--playlist is given, the window loops its scenes and
# shows per-talker text from pctools/audio_pipe (built here unless
# PDM_NO_BUILD=1; without its whisper model it starts without text).
# Transcript log: ~/pdm-eth-booth.log.
#
# Usage:
#   pdm-eth-demo                       live window on enp129s0
#   pdm-eth-demo --emulator            no board: built-in emulator
#   pdm-eth-demo --ch0 a.wav --ch1 b.wav   extra args go to "pdm-eth gui"
#   PDM_IFACE=enp5s0 pdm-eth-demo      another NIC
#   PDM_NO_BUILD=1 pdm-eth-demo        skip the cargo build step
#   PDM_PLAYLIST=~/scenes pdm-eth-demo another booth playlist folder
#   PDM_UI_SCALE=1.75 pdm-eth-demo   larger window text (32" 4K booth screen)
#
# Install once (works via the symlink, it resolves its own location):
#   ln -sf "$(readlink -f pdm-eth-demo.sh)" ~/.local/bin/pdm-eth-demo
set -euo pipefail

APP_DIR=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")
PDM=$APP_DIR/target/release/pdm-eth
IFACE=${PDM_IFACE:-enp129s0}
CAPS=cap_net_admin,cap_net_raw=ep

say() { printf '\033[1m[pdm-eth-demo]\033[0m %s\n' "$*"; }
die() { printf '\033[1;31m[pdm-eth-demo]\033[0m %s\n' "$*" >&2; exit 1; }

AP_DIR=$(readlink -f "$APP_DIR/../pctools/audio_pipe")
AP=$AP_DIR/target/release/audio_pipe
MODEL=$AP_DIR/models/ggml-large-v3-turbo-q5_0.bin
PLAYLIST=${PDM_PLAYLIST:-$APP_DIR/audio/booth}

emulator=0
own_input=0
for a in "$@"; do
    case $a in
        --emulator) emulator=1 ;;
        --ch[0-7]|--ch[0-7]=*|--set|--playlist|--playlist=*) own_input=1 ;;
        --iface|--iface=*) die "set the NIC with PDM_IFACE=<name>, not --iface" ;;
        -h|--help) sed -n '2,26p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
    esac
done

# 1. Build (cargo only relinks when a source changed).
if [ "${PDM_NO_BUILD:-0}" != 1 ]; then
    command -v cargo >/dev/null || die "cargo not found (install Rust with rustup)"
    say "cargo build --release"
    cargo build --release --quiet --manifest-path "$APP_DIR/Cargo.toml"
fi
[ -x "$PDM" ] || die "$PDM missing: run without PDM_NO_BUILD"

# 2. Raw-socket capabilities (every relink drops them).
if [ "$emulator" = 0 ] && ! getcap "$PDM" | grep -q "$CAPS"; then
    say "setting raw-socket capabilities (sudo)"
    sudo setcap cap_net_raw,cap_net_admin+ep "$PDM"
fi

# 3. NIC: unmanaged by NetworkManager, up, link check.
if [ "$emulator" = 0 ]; then
    ip link show "$IFACE" >/dev/null 2>&1 || \
        die "no interface $IFACE (ip -br link lists them; set PDM_IFACE)"
    if command -v nmcli >/dev/null; then
        state=$(nmcli -g GENERAL.STATE device show "$IFACE" 2>/dev/null || true)
        if [ -n "$state" ] && [[ "$state" != 10* ]]; then
            say "NetworkManager: $IFACE managed no (sudo)"
            sudo nmcli device set "$IFACE" managed no
        fi
    fi
    if ! ip -br link show "$IFACE" | grep -q '\bUP\b'; then
        say "ip link set $IFACE up (sudo)"
        sudo ip link set "$IFACE" up
        sleep 3   # Gigabit autonegotiation
    fi
    if [ "$(cat "/sys/class/net/$IFACE/carrier" 2>/dev/null || echo 0)" = 1 ]; then
        speed=$(cat "/sys/class/net/$IFACE/speed" 2>/dev/null || echo "?")
        say "link on $IFACE: ${speed} Mb/s"
        [ "$speed" = 1000 ] || say "warning: expected 1000 Mb/s"
    else
        say "warning: no link on $IFACE (cable in J18? R5F running?); the window starts anyway"
    fi
fi

# 4. Booth playlist and transcription.
booth=()
if [ "$own_input" = 0 ] && [ -d "$PLAYLIST" ]; then
    booth=(--playlist "$PLAYLIST")
    text=1
    if [ "${PDM_NO_BUILD:-0}" != 1 ]; then
        say "building audio_pipe (CUDA)"
        if ! PATH=/usr/local/cuda/bin:$PATH CMAKE_CUDA_ARCHITECTURES=${CMAKE_CUDA_ARCHITECTURES:-120} \
                cargo build --release --quiet --manifest-path "$AP_DIR/Cargo.toml"; then
            say "warning: audio_pipe build failed: starting without transcription"
            text=0
        fi
    fi
    if [ ! -x "$AP" ]; then
        say "warning: $AP missing: starting without transcription"
        text=0
    elif [ ! -f "$MODEL" ]; then
        say "warning: whisper model missing: starting without transcription. Download once:"
        say "  curl -L -o $MODEL https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo-q5_0.bin"
        text=0
    fi
    if [ "$text" = 1 ]; then
        booth+=(--transcribe "$AP" --transcript-log "$HOME/pdm-eth-booth.log")
    fi
    say "booth playlist $PLAYLIST$([ "$text" = 1 ] && echo ", transcription on")"
elif [ "$own_input" = 0 ]; then
    say "no playlist folder $PLAYLIST: booth clips (make_booth_scenes.sh creates it)"
fi

# 5. The live window.
if [ "$emulator" = 1 ]; then
    say "pdm-eth gui ${booth[*]} $*"
    exec "$PDM" gui "${booth[@]}" "$@"
fi
say "pdm-eth gui --iface $IFACE ${booth[*]} $*"
exec "$PDM" gui --iface "$IFACE" "${booth[@]}" "$@"
