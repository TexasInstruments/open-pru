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

# talker <id> <voice> <az> <dist m> <dB> <start s>
talker() { printf '{"id":"%s","voice":"%s","az":%s,"el":0,"dist":%s,"db":%s,"start":%s,"target":true}' "$@"; }

# scene <name> <duration s> <talker json>...: the factory scenario, the two TTS
# workers in the male_low (worker A, 0.85x) and female_high (worker B) slots
scene() {
    local name=$1 dur=$2; shift 2
    if [ ${#want[@]} -gt 0 ] && [[ ! " ${want[*]} " =~ " $name " ]]; then return; fi
    local d=$OUT/$name sp
    sp=$(IFS=,; echo "$*")
    mkdir -p "$d"
    echo "== $name"
    node "$HERE/render_mics.js" factory "$d" "{\"_dur\":$dur,\"speakers\":[$sp],\
\"_voices\":{\"male_low\":\"$V/worker_a.wav\",\"female_high\":\"$V/worker_b.wav\"},\"_rate\":{\"male_low\":0.85},\"_dry_dir\":\"$d/dry\"}"
}
want=("$@")

# factory_a: the 2026-11-07 scene factory_4workers_9s5_b270_f150 (female not yet exact: Task 10)
scene factory_a 9.5 \
    "$(talker s1 male 30 1.5 0 0.3)" \
    "$(talker s2 female 150 2.0 -2 0.7)" \
    "$(talker s3 male_low 210 1.8 -1 1.1)" \
    "$(talker s4 female_high 270 2.5 -3 1.5)"

echo "scenes in $OUT"
