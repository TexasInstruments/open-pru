#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
# Re-render each test scene with "_ego_take" so <scene>/ego/mic1..8.wav holds the robot-only recording at the same gain,
# and <scene>/dry/ the talkers' dry sources (reference for the SDR score in tools/bench.py).
# The mix is re-rendered into a temp dir and must be byte-identical to the existing scene (checks the gain matches).
set -e
cd "$(dirname "$0")/.."
export SCENE_ROOT=${SCENE_ROOT:-$(cd .. && pwd)}  # folder with the audio_wav* scenes and scenes/web
S=$SCENE_ROOT
TMP=$(mktemp -d)
take() {  # take <scenario> <scene dir> [json overrides without braces]
  local o="{\"_ego_take\":\"$2/ego\",\"_dry_dir\":\"$2/dry\"${3:+,$3}}"
  node tools/render_mics.js "$1" "$TMP/mix" "$o" >/dev/null
  for m in 1 2 3 4 5 6 7 8; do cmp -s "$TMP/mix/mic$m.wav" "$2/mic$m.wav" || { echo "MISMATCH $2 mic$m"; exit 1; }; done
  echo "ok $2"
}
TD=${TESTDATA:-tools/testdata}
for s in home factory hospital automobile quiet robot; do [ -d "$TD/scene_$s" ] && take $s "$TD/scene_$s"; done
[ -d $S/audio_wav_factory ] && take factory $S/audio_wav_factory
V='"_dur":9.5,"_voices":{"male_low":"tools/voices/worker_a.wav","female_high":"tools/voices/worker_b.wav"},"_rate":{"male_low":0.85}'
S4='"speakers":[{"id":"s1","voice":"male","az":30,"el":0,"dist":1.5,"db":0,"start":0.3,"target":true},{"id":"s2","voice":"male_low","az":110,"el":0,"dist":2.0,"db":0,"start":1.8,"target":true},{"id":"s3","voice":"female","az":200,"el":0,"dist":2.0,"db":0,"start":4.6,"target":true},{"id":"s4","voice":"female_high","az":290,"el":0,"dist":1.8,"db":0,"start":6.5,"target":true}]'
take factory $S/audio_wav_factory4 "$V,$S4"
take factory $S/audio_wav_factory4_n10 "$V,\"_noise_db\":-10,$S4"
VD='"_dur":6.5,"_voices":{"male_low":"tools/voices/worker_a.wav","female_high":"tools/voices/worker_b.wav"},"_rate":{"male_low":0.85}'
SD='"speakers":[{"id":"s1","voice":"male","az":30,"el":0,"dist":1.5,"db":0,"start":0.3,"target":true},{"id":"s2","voice":"male_low","az":110,"el":0,"dist":2.0,"db":0,"start":1.0,"target":true},{"id":"s3","voice":"female","az":200,"el":0,"dist":2.0,"db":0,"start":1.7,"target":true},{"id":"s4","voice":"female_high","az":290,"el":0,"dist":1.8,"db":0,"start":2.4,"target":true}]'
take factory $S/audio_wav_factory4_dense "$VD,\"_noise_db\":-10,$SD"
S240=${SD/\"az\":200/\"az\":240}; take factory $S/audio_wav_factory4_dense240 "$VD,\"_noise_db\":-10,${S240/\"az\":290/\"az\":310}"
rm -rf "$TMP"
