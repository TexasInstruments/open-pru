// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
// node render_mics.js <scene> <outdir> ['{"json":"overrides"}']  -> outdir/mic1..micM.wav (16-bit) + settings.json
"use strict";
const fs = require("fs"), path = require("path"), vm = require("vm");
// SCENE_ROOT: folder holding the Scene Simulator checkout as scenes/ (its web/src/engine.js is run here)
if (!process.env.SCENE_ROOT) { console.error("set SCENE_ROOT to the folder that holds scenes/web"); process.exit(2); }
const web = path.join(process.env.SCENE_ROOT, "scenes", "web"), root = path.join(web, "..");
const ctx = { console, Math, Date, Float64Array, Float32Array, Uint32Array, Array, Object, JSON, Number, String, Set };
vm.createContext(ctx);
// optional overrides (in-memory only, the repo is not modified):
//   "_dur": scene length in s (engine default 5.5). The current engine takes it as the "duration" setting (1-120 s,
//           start clamp and noise loops follow it); an engine without "duration" (before scenes 2026-10) is patched
//           in source as before. Either way the rendered length is checked.
//   "_voices": {"male_low": "a.wav", "female_high": "b.wav"} replace those two voice slots with other 16 kHz clips
//   "_rate": {"male_low": 0.85} resampling factor applied to a replaced clip (pitch/tempo change)
const [scene, out, extra] = process.argv.slice(2);
const ov = extra ? JSON.parse(extra) : {};
let src = ["engine.js", "scenarios.js"].map(f => fs.readFileSync(path.join(web, "src", f), "utf8")).join("\n");
const DUR_WANT = ov._dur ? Number(ov._dur) : null;
const DUR_PARAM = src.includes("engSetDuration(");   // current engine: length is the "duration" setting
if (DUR_WANT && !DUR_PARAM) {
  if (!src.includes("DUR = 5.5,")) throw new Error("engine changed: neither engSetDuration() nor 'DUR = 5.5,' found");
  const d = DUR_WANT;
  src = src.replace("DUR = 5.5,", `DUR = ${d},`).replace("engClamp(Number(s.start) || 0, 0, 4.5)", `engClamp(Number(s.start) || 0, 0, ${d - 1})`)
    .replace(/(t|c) < 5\.5;/g, "$1 < DUR_SC();");
}
//   "_ego_take": "<dir>" also write the robot-only take (robot noises with another random seed + the sensor noise floor,
//               no talkers, no ambient sources; what the robot records with nobody around) to <dir>/mic1..8.wav,
//               scaled by the same output gain as the mix, so its level matches. Scenes without robot noise: floor only.
if (ov._ego_take) {
  const k = "    accumulateCov(take, Tr, Ti, 1);";
  if (!src.includes(k)) throw new Error("engine changed: robot take hook not found");
  src = src.replace(k, "    globalThis.__take = take;\n" + k)
    .replace("  const anyRobot = ROBOT_KEYS.some(k => p[k + \"On\"]);", "  const anyRobot = ROBOT_KEYS.some(k => p[k + \"On\"]);\n  if (!anyRobot) globalThis.__take = roomNoise.map(c => Float64Array.from(c).reverse());");
}
vm.runInContext(src + "\n;globalThis.__x = { render, readWav, normalizeParams, SCENARIOS };", ctx);
const { render, readWav, normalizeParams, SCENARIOS } = ctx.__x;
const DEF = { rt60: 0.5, radius: 5, roomDb: 25, manAng: 40, womanAng: 150, dist: 1.5, babyOn: true, babyAng: 250, babyDist: 2.5, babyDb: -6,
  fanOn: true, fanDb: -6, fanRpm: 4200, neckOn: true, neckDb: -4, shouldersOn: true, shouldersDb: -8, footstepsOn: false, footstepsDb: -2,
  clicksOn: true, clicksDb: -10, humOn: true, humDb: -25 };
const OFF = { fanOn: false, neckOn: false, shouldersOn: false, footstepsOn: false, clicksOn: false, humOn: false };
const PRESETS = { baby: OFF, fan: { ...OFF, fanOn: true, humOn: true }, robot: {}, quiet: { ...OFF, babyOn: false } };
function paramsFor(scene) {
  if (SCENARIOS[scene]) return JSON.parse(JSON.stringify(SCENARIOS[scene].params));
  const n = normalizeParams({ ...DEF, ...PRESETS[scene], scenario: "custom" }); delete n._legacy; delete n._warnings; return { ...n, noises: [] };
}
function wav16(x) {
  const n = x.length, b = Buffer.alloc(44 + 2 * n);
  b.write("RIFF", 0); b.writeUInt32LE(36 + 2 * n, 4); b.write("WAVE", 8); b.write("fmt ", 12); b.writeUInt32LE(16, 16);
  b.writeUInt16LE(1, 20); b.writeUInt16LE(1, 22); b.writeUInt32LE(16000, 24); b.writeUInt32LE(32000, 28); b.writeUInt16LE(2, 32); b.writeUInt16LE(16, 34);
  b.write("data", 36); b.writeUInt32LE(2 * n, 40);
  for (let i = 0; i < n; i++) b.writeInt16LE(Math.max(-32768, Math.min(32767, Math.round(x[i] * 32767))), 44 + 2 * i);
  return b;
}
const ab = f => { const b = fs.readFileSync(path.join(root, "voices", f)); return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength); };
const voices = { male: readWav(ab("male.wav"), true).x, female: readWav(ab("female.wav"), true).x, baby: readWav(ab("baby.wav"), false).x };
for (const [slot, file] of Object.entries(ov._voices || {})) {
  const b = fs.readFileSync(file), x = readWav(b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength), true).x;
  voices[slot] = x;
}
if (ov._voices) {   // engVoice() derives male_low/female_high from male/female; use the supplied clips instead
  ctx.__vo = voices; ctx.__rate = ov._rate || {};
  vm.runInContext(`engVoice = function (name, voices, cache) {
    if (__vo[name] && (name === "male_low" || name === "female_high"))
      return cache[name] || (cache[name] = __rate[name] ? engResample(__vo[name], __rate[name]) : __vo[name]);
    if (name === "male_low") return cache.male_low || (cache.male_low = engResample(voices.male, 0.85));
    if (name === "female_high") return cache.female_high || (cache.female_high = engResample(voices.female, 1.15));
    return voices[name];
  };`, ctx);
}
//   "_noise_db": dB offset applied to every ambient noise source and to the robot's own noises
const p = { ...paramsFor(scene), ...Object.fromEntries(Object.entries(ov).filter(([k]) => !k.startsWith("_"))) };
if (DUR_WANT && DUR_PARAM) p.duration = DUR_WANT;
if (ov._noise_db) {
  p.noises = (p.noises || []).map(n => ({ ...n, db: n.db + ov._noise_db }));
  for (const k of ["fan", "neck", "shoulders", "footsteps", "clicks", "hum"]) if (p[k + "Db"] !== undefined) p[k + "Db"] += ov._noise_db;
}
const res = render(p, voices);
if (DUR_WANT && res.mics[0].length !== Math.round(DUR_WANT * 16000))
  throw new Error(`_dur ${DUR_WANT} s requested, rendered ${res.mics[0].length / 16000} s (engine clamps "duration" to 1-120 s)`);
fs.mkdirSync(out, { recursive: true });
let pk = 0; for (const c of res.mics) for (const v of c) pk = Math.max(pk, Math.abs(v));
const g = pk > 0.99 ? 0.99 / pk : 1;
res.mics.forEach((c, i) => fs.writeFileSync(path.join(out, `mic${i + 1}.wav`), wav16(Array.from(c, v => v * g))));
//   "_dry_dir": "<dir>" write each talker's dry source signal, placed at its start time, as <dir>/<speaker id>_<voice>.wav
if (ov._dry_dir) {
  fs.mkdirSync(ov._dry_dir, { recursive: true });
  for (const sp of p.speakers) if (res.dry[sp.id]) {
    const d = res.dry[sp.id]; let m = 0; for (const v of d) m = Math.max(m, Math.abs(v));
    fs.writeFileSync(path.join(ov._dry_dir, `${sp.id}_${sp.voice}.wav`), wav16(Array.from(d, v => v * 0.9 / (m || 1))));
  }
}
if (ov._ego_take) {
  fs.mkdirSync(ov._ego_take, { recursive: true });
  ctx.__take.forEach((c, i) => fs.writeFileSync(path.join(ov._ego_take, `mic${i + 1}.wav`), wav16(Array.from(c, v => v * g))));
}
fs.writeFileSync(path.join(out, "settings.json"), JSON.stringify({ ...p, micPositions: res.micPositions, sources: res.sources }, null, 1));
console.log(scene, "->", out, "mics", res.mics.length, "peak", pk.toFixed(3), "speakers", JSON.stringify(p.speakers.map(s => [s.voice, s.az, s.dist, s.start])));
