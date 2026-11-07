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
# tools/booth_curate.py passed it on the board (5 passes, every talker exact).
set -euo pipefail
HERE=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")
OUT=${BOOTH_OUT:-$(readlink -f "$HERE/../../../pc_app")/audio/booth}
V=$HERE/voices
: "${SCENE_ROOT:?set SCENE_ROOT to the folder that holds scenes/web}"

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
\"_rate\":{\"male_low\":0.85},\"_dry_dir\":\"$d/dry\"}"
}
want=("$@")
C="The forklift is waiting at gate two."

# The directions are where each voice comes out exact (machine 90°, press 330°,
# AGV 120°->40°, robot 1 60°->170°, conveyor 180° make the others hard).
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
