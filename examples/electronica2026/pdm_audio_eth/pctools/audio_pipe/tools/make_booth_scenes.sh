#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
#
# Renders the curated booth scenes (booth transcripts design 3) into
# pc_app/audio/booth/<name>/ (mic1..8.wav, settings.json, dry/), the playlist
# pdm-eth-demo plays. The folder is not in git: the male and female voices
# come from the Scene Simulator's scenes/voices/, whose licence is not
# documented, so every PC renders its own copy with this script.
#
# Needs node and SCENE_ROOT = the folder that holds the Scene Simulator
# checkout as scenes/ (gh repo clone accio-sunshine/scenes).
#   SCENE_ROOT=~/GitHub/audio_pipe_scenes tools/make_booth_scenes.sh [name ...]
# Without names every scene is rendered. Each scene is kept only after
# tools/booth_curate.py passed it on the board (5 passes, every talker exact;
# factory_a and factory_b: every pass exact, 8 and 9 passes, 2026-11-08).
set -euo pipefail
HERE=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")
OUT=${BOOTH_OUT:-$(readlink -f "$HERE/../../../pc_app")/audio/booth}
V=$HERE/voices
: "${SCENE_ROOT:?set SCENE_ROOT to the folder that holds scenes/web}"

# The scenes were checked on the board with the Scene Simulator at 20f157b
# (2026-11-08, Kokoro voices + Factory booth) or later. An older engine renders
# different audio: with a 2026-10-12 checkout worker A came out wrong on every
# pass (2026-11-09). BOOTH_ANY_SIM=1 renders with any checkout.
SIM_MIN=20f157b
SIM=$SCENE_ROOT/scenes
if [ "${BOOTH_ANY_SIM:-0}" != 1 ]; then
    if ! git -C "$SIM" rev-parse -q --verify HEAD >/dev/null 2>&1; then
        echo "make_booth_scenes: $SIM is not a git checkout, cannot check its version (BOOTH_ANY_SIM=1 to render anyway)" >&2
        exit 1
    fi
    if ! git -C "$SIM" merge-base --is-ancestor "$SIM_MIN" HEAD 2>/dev/null; then
        echo "make_booth_scenes: the Scene Simulator in $SIM ($(git -C "$SIM" log -1 --format='%h %cs' HEAD)) is older than $SIM_MIN (2026-11-08)." >&2
        echo "  Use a current clone: git clone https://github.com/accio-sunshine/scenes \$SCENE_ROOT/scenes (or git pull there)." >&2
        exit 1
    fi
fi

# talker <id> <voice> <az> <dist m> <dB> <start s> [reference text, for a replaced clip]
talker() {
    printf '{"id":"%s","voice":"%s","az":%s,"el":0,"dist":%s,"db":%s,"start":%s,"target":true' "$1" "$2" "$3" "$4" "$5" "$6"
    [ -n "${7:-}" ] && printf ',"text":"%s"' "$7"
    printf '}'
}

# scene <name> <duration s> <talker json>...: the factory scenario with three
# TTS voices (tools/voices, SpeechBrain Tacotron2 + HiFi-GAN, LJSpeech):
# worker A (0.85x) in the male_low slot, worker B in female_high, worker C
# (1.1x) in the female slot; the male is the Scene Simulator's male.wav. The
# Scene Simulator's female.wav is not used: in this hall (RT60 1.2 s) whisper
# garbles her at every position tried (2026-11-07).
scene() {
    local name=$1 dur=$2; shift 2
    if [ ${#want[@]} -gt 0 ] && [[ ! " ${want[*]} " =~ " $name " ]]; then return; fi
    local d=$OUT/$name sp
    sp=$(IFS=,; echo "$*")
    mkdir -p "$d"
    echo "== $name"
    node "$HERE/render_mics.js" factory "$d" "{\"_dur\":$dur,\"speakers\":[$sp],\
\"_voices\":{\"male_low\":\"$V/worker_a.wav\",\"female_high\":\"$V/worker_b.wav\",\"female\":\"$V/worker_c.wav\"},\
\"_rate\":{\"male_low\":0.85},\"_dry_dir\":\"$d/dry\",\"noises\":$NOISES}"
}
want=("$@")
C="The forklift is waiting at gate two."

# The factory preset's noise sources, with the stamping press moved from 330°
# to 150°: through the board the tracker pulled worker C (0°) onto the press at
# 330° and whisper garbled her on most passes (2026-11-08). Everything else is
# the preset (machine 90°, air hiss 250°, AGV 120°->40°, robot 1 60°->170°,
# robot 2 300°->220°, conveyor 180°).
NOISES='[{"id":"machine","label":"Machine hum + drive whine","gen":"machine_hum","az":90,"el":20,"dist":6,"db":-6,"on":true,"path":null},{"id":"press","label":"Stamping press","gen":"press","az":150,"el":10,"dist":7,"db":-3,"on":true,"path":null},{"id":"airhiss","label":"Compressed-air hiss","gen":"airhiss","az":250,"el":40,"dist":4,"db":-10,"on":true,"path":null},{"id":"agv","label":"AGV reverse beeper (moves)","gen":"agv_beeper","az":120,"el":-5,"dist":7,"db":-8,"on":true,"path":{"az1":40,"el1":-5,"dist1":6}},{"id":"robot1","label":"Other robot 1 (moves)","gen":"robot_a","az":60,"el":-5,"dist":5,"db":-9,"on":true,"path":{"az1":170,"el1":-5,"dist1":3.5}},{"id":"robot2","label":"Other robot 2 (moves)","gen":"robot_b","az":300,"el":-5,"dist":6,"db":-9,"on":true,"path":{"az1":220,"el1":-5,"dist1":4}},{"id":"conveyor","label":"Conveyor rumble + clacks","gen":"conveyor","az":180,"el":-10,"dist":5,"db":-9,"on":true,"path":null}]'

# The directions are where each voice comes out exact.
# factory_a: male first, then worker C, A, B (0.4 s apart, overlapping)
scene factory_a 9.5 \
    "$(talker s1 male 30 1.5 0 0.3)" \
    "$(talker s2 female 0 1.5 0 0.7 "$C")" \
    "$(talker s3 male_low 210 1.8 -1 1.1)" \
    "$(talker s4 female_high 270 2.5 -3 1.5)"

# factory_b: the same places, the other order (worker B first, the male last)
scene factory_b 9.5 \
    "$(talker s1 female_high 270 2.5 -3 0.3)" \
    "$(talker s2 male_low 210 1.8 -1 0.7)" \
    "$(talker s3 female 0 1.5 0 1.1 "$C")" \
    "$(talker s4 male 30 1.5 0 1.5)"

echo "scenes in $OUT"
