#![allow(dead_code)]
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
mod asr;
mod common;
mod doa;
mod ghdss;
mod health;
mod music;
mod noise;
mod pipeline;
mod shm;
mod refine;
mod sep;
mod stft;
mod wpe;

use anyhow::Result;
use clap::{Args, Parser, Subcommand};
use common::*;
use std::path::PathBuf;
use std::time::{Duration, Instant};

#[derive(Parser)]
#[command(about = "8-mic robot audio front-end: per-speaker streams + ASR (WPE, DOA tracking, mask-MVDR, whisper/CUDA)")]
struct Cli {
    #[command(subcommand)]
    cmd: Cmd,
}

#[derive(Subcommand)]
enum Cmd {
    /// Run the full pipeline on a folder with mic1.wav .. mic8.wav
    Run(RunArgs),
    /// Transcribe mono 16 kHz WAV files directly (reference / baseline)
    Asr {
        files: Vec<PathBuf>,
        #[arg(long, default_value = "models/ggml-large-v3-turbo-q5_0.bin")]
        model: PathBuf,
        #[arg(long)]
        cpu: bool,
    },
}

#[derive(Args)]
struct RunArgs {
    /// folder containing mic1.wav .. mic8.wav (16 kHz)
    input: PathBuf,
    #[arg(long, default_value = "out")]
    out: PathBuf,
    /// pace input at real time (measures true end-of-speech -> text latency)
    #[arg(long)]
    realtime: bool,
    /// skip the start-up channel health check
    #[arg(long)]
    no_health: bool,
    /// print per-stage CPU time and state memory (always written to <out>/profile.json)
    #[arg(long)]
    profile: bool,
    #[command(flatten)]
    p: PipeArgs,
}

/// Flags that set up the pipeline (shared by `run` and `live`).
#[derive(Args, Clone)]
struct PipeArgs {
    #[arg(long, default_value = "models/ggml-large-v3-turbo-q5_0.bin")]
    model: PathBuf,
    /// skip ASR (DSP only)
    #[arg(long)]
    no_asr: bool,
    /// run ASR on the CPU instead of CUDA
    #[arg(long)]
    cpu_asr: bool,
    #[arg(long, default_value = "en")]
    language: String,
    /// array radius in metres (Scene Simulator default 5 cm)
    #[arg(long, default_value_t = 0.05)]
    radius: f32,
    /// WPE taps K (0 disables WPE)
    #[arg(long, default_value_t = 5)]
    wpe_taps: usize,
    #[arg(long, default_value_t = 2)]
    wpe_delay: usize,
    #[arg(long, default_value_t = 0.999)]
    wpe_alpha: f32,
    /// maximum number of talkers
    #[arg(long, default_value_t = 6)]
    max_speakers: usize,
    /// microphones to switch off (1-based), e.g. --mics-off 1
    #[arg(long, value_delimiter = ',')]
    mics_off: Vec<usize>,
    /// known ego-noise directions to ignore, e.g. --exclude 180:20 (azimuth:half-width)
    #[arg(long, value_delimiter = ',')]
    exclude: Vec<String>,
    #[arg(long, default_value_t = 0.003)]
    peak_min: f32,
    #[arg(long, default_value_t = 0.008)]
    birth_min: f32,
    #[arg(long, default_value_t = 0.5)]
    coh_min: f32,
    /// history replayed into a newly detected talker's stream (recovers speech before track confirmation)
    #[arg(long, default_value_t = 2500)]
    replay_ms: usize,
    /// diagonal loading of the noise covariance used for whitening (relative to its mean eigenvalue)
    #[arg(long, default_value_t = 0.001)]
    white_load: f32,
    #[arg(long, default_value_t = 20.0)]
    kappa: f32,
    #[arg(long, default_value_t = 0.5)]
    s_bg: f32,
    #[arg(long, default_value_t = 0.02)]
    loading: f32,
    #[arg(long, default_value_t = 0.15)]
    pf_gmin: f32,
    #[arg(long, default_value_t = 0.25)]
    sp_floor: f32,
    /// endpointer thresholds on per-speaker activity
    #[arg(long, default_value_t = 0.10)]
    vad_on: f32,
    #[arg(long, default_value_t = 0.05)]
    vad_off: f32,
    /// silence (ms) that ends an utterance
    #[arg(long, default_value_t = 320)]
    hang_ms: usize,
    /// emit partial hypotheses every N ms during an utterance (0 = off)
    #[arg(long, default_value_t = 0)]
    partial_ms: usize,
    /// neural second pass per utterance (SpeechBrain SepFormer sidecar on the GPU)
    #[arg(long, value_enum, default_value_t = refine::NeuralMode::Off)]
    neural: refine::NeuralMode,
    /// python interpreter with speechbrain installed
    #[arg(long, default_value = "py/.venv/bin/python")]
    neural_python: PathBuf,
    #[arg(long, default_value = "py/mask_server.py")]
    neural_script: PathBuf,
    /// floor of the neural post-mask after the refined MVDR (1 = no post-mask)
    #[arg(long, default_value_t = 0.2)]
    neural_post_floor: f32,
    /// context before an utterance given to the separator (ms)
    #[arg(long, default_value_t = 500)]
    neural_ctx_ms: usize,
    #[arg(short, long)]
    verbose: bool,
    /// separation method: mask-MVDR, or HARK's GHDSS
    #[arg(long, value_enum, default_value_t = ghdss::SepMode::Mvdr)]
    sep: ghdss::SepMode,
    /// GHDSS nonlinearity scale η in φ(y) = tanh(η|y|) e^{j∠y}
    #[arg(long, default_value_t = 1.0)]
    ghdss_eta: f32,
    /// GHDSS domain: noise-whitened, or raw (scaled by the noise level, as HARK)
    #[arg(long, value_enum, default_value_t = ghdss::GhdssDomain::White)]
    ghdss_domain: ghdss::GhdssDomain,
    /// GHDSS step-size scale (1 = HARK's adaptive 1/8 rule)
    #[arg(long, default_value_t = 1.0)]
    ghdss_mu: f32,
    /// start new GHDSS talkers from delay-and-sum (HARK) instead of their mask-MVDR solution
    #[arg(long)]
    ghdss_init_ds: bool,
    /// refine each talker's azimuth with a local SEVD-MUSIC search (HARK LocalizeMUSIC, 500-2800 Hz)
    #[arg(long)]
    music_refine: bool,
    /// MUSIC signal-subspace size (HARK NUM_SOURCE)
    #[arg(long, default_value_t = 2)]
    music_ns: usize,
    /// MUSIC search half-width around the tracked azimuth (deg)
    #[arg(long, default_value_t = 20.0)]
    music_span: f32,
    /// MUSIC covariance window (frames; HARK WINDOW = 50)
    #[arg(long, default_value_t = 50.0)]
    music_win: f32,
    /// folder with a robot-only take (mic1..8.wav: ego noise with nobody talking) -> fixed ego-noise covariance
    #[arg(long)]
    ego_noise: Option<PathBuf>,
    /// how the fixed ego-noise covariance is used
    #[arg(long, value_enum, default_value_t = noise::EgoMode::Prior)]
    ego_mode: noise::EgoMode,
    /// prior/doa: weight of Φego in Φn in frames (the adaptive part remembers ≈100 frames); add/add-doa: gain g on Φego
    #[arg(long, default_value_t = 100.0)]
    ego_weight: f32,
}

/// Φego(f) = mean x̂ x̂ᴴ over a robot-only take, after the same STFT + WPE as the live path.
fn ego_covariance(dir: &std::path::Path, active: &[bool], wpe_prm: wpe::WpeParams) -> Result<Vec<Mat8>> {
    let mut ch = read_mics(dir)?;
    for m in 0..M {
        if !active[m] {
            ch[m].iter_mut().for_each(|v| *v = 0.0);
        }
    }
    let mut ana = stft::Analyzer::new();
    let mut wpe = wpe::Wpe::new(wpe_prm);
    let mut acc = vec![ZM; F];
    let mut y: Frame = vec![ZV; F];
    let frames = ch[0].len() / H;
    let skip = 32;
    anyhow::ensure!(frames > skip + 50, "ego-noise take too short ({frames} frames)");
    for t in 0..frames {
        let hops: Vec<&[f32]> = ch.iter().map(|c| &c[t * H..(t + 1) * H]).collect();
        ana.push(&hops, &mut y);
        wpe.process(&mut y);
        if t >= skip {
            for f in 0..F {
                rank1_acc(&mut acc[f], &y[f], 1.0);
            }
        }
    }
    for a in acc.iter_mut() {
        scale_mat(a, 1.0 / (frames - skip) as f32);
    }
    Ok(acc)
}

fn peak_rss_kb() -> u64 {
    std::fs::read_to_string("/proc/self/status")
        .ok()
        .and_then(|s| s.lines().find(|l| l.starts_with("VmHWM:")).and_then(|l| l.split_whitespace().nth(1)?.parse().ok()))
        .unwrap_or(0)
}

/// Per-stage profile: mean / p99 µs per frame, share of the 8 ms hop, state bytes; plus mean time by stream count.
fn write_profile(pipe: &pipeline::Pipeline, out: &std::path::Path, print: bool) -> Result<()> {
    use pipeline::{NST, STAGES};
    let n = pipe.prof.len().max(1);
    let mem = pipe.mem_bytes();
    let hop_us = H as f64 / FS as f64 * 1e6;
    let mut stages = Vec::new();
    if print {
        eprintln!("\nper-stage profile ({} frames, hop {:.0} µs, rayon threads {}):", n, hop_us, rayon::current_num_threads());
        eprintln!("  {:10} {:>9} {:>9} {:>9} {:>8} {:>10}", "stage", "mean µs", "p99 µs", "max µs", "% hop", "state KiB");
    }
    for k in 0..NST {
        let mut v: Vec<f32> = pipe.prof.iter().map(|(t, _)| t[k]).collect();
        v.sort_by(|a, b| a.total_cmp(b));
        let mean = v.iter().map(|&x| x as f64).sum::<f64>() / n as f64;
        let q = |p: f64| v[((v.len() - 1) as f64 * p) as usize] as f64;
        if print {
            eprintln!("  {:10} {:9.1} {:9.1} {:9.1} {:7.2}% {:10.1}", STAGES[k], mean, q(0.99), q(1.0), 100.0 * mean / hop_us, mem[k] as f64 / 1024.0);
        }
        stages.push(serde_json::json!({"stage": STAGES[k], "mean_us": mean, "p50_us": q(0.5), "p99_us": q(0.99), "max_us": q(1.0), "state_bytes": mem[k]}));
    }
    // mean per-stage time grouped by the number of live streams
    let maxc = pipe.prof.iter().map(|(_, c)| *c as usize).max().unwrap_or(0);
    let mut by = serde_json::Map::new();
    for c in 0..=maxc {
        let sel: Vec<&[f32; NST]> = pipe.prof.iter().filter(|(_, k)| *k as usize == c).map(|(t, _)| t).collect();
        if sel.is_empty() {
            continue;
        }
        let means: Vec<f64> = (0..NST).map(|k| sel.iter().map(|t| t[k] as f64).sum::<f64>() / sel.len() as f64).collect();
        by.insert(c.to_string(), serde_json::json!({"frames": sel.len(), "mean_us": means}));
    }
    let pcm_bytes: usize = pipe.streams.iter().map(|s| s.pcm.len() * 4).sum();
    let total: f64 = pipe.frame_us.iter().map(|&x| x as f64).sum::<f64>() / n as f64;
    let rss = peak_rss_kb();
    if print {
        eprintln!("  {:10} {:9.1} {:>9} {:>9} {:7.2}% {:10.1}", "total", total, "", "", 100.0 * total / hop_us, mem.iter().sum::<usize>() as f64 / 1024.0);
        eprintln!("  streams at end {}, output pcm {:.0} KiB, process peak RSS {:.1} MiB", pipe.streams.len(), pcm_bytes as f64 / 1024.0, rss as f64 / 1024.0);
    }
    let j = serde_json::json!({
        "frames": n, "hop_us": hop_us, "rayon_threads": rayon::current_num_threads(),
        "streams_end": pipe.streams.len(), "stages": stages, "by_streams": by,
        "frame_mean_us": total, "output_pcm_bytes": pcm_bytes, "peak_rss_kb": rss,
    });
    std::fs::write(out.join("profile.json"), serde_json::to_string_pretty(&j)?)?;
    Ok(())
}

/// Relative model paths are tried as given, then relative to the crate root (target/release/../../).
fn resolve_model(p: &std::path::Path) -> PathBuf {
    if p.is_absolute() || p.exists() {
        return p.to_path_buf();
    }
    if let Ok(exe) = std::env::current_exe() {
        if let Some(root) = exe.parent().and_then(|d| d.parent()).and_then(|d| d.parent()) {
            let q = root.join(p);
            if q.exists() {
                return q;
            }
        }
    }
    p.to_path_buf()
}

fn ms_frames(ms: usize) -> usize {
    (ms * FS / 1000).div_ceil(H)
}

fn main() -> Result<()> {
    let cli = Cli::parse();
    match cli.cmd {
        Cmd::Asr { files, model, cpu } => {
            let t = Instant::now();
            let mut tr = asr::Transcriber::new(&resolve_model(&model), !cpu, "en")?;
            tr.transcribe(&vec![0.0; FS])?;
            eprintln!("model load + warm-up {:.0} ms", t.elapsed().as_secs_f64() * 1e3);
            for f in files {
                let (x, fs) = read_wav(&f)?;
                anyhow::ensure!(fs == FS as u32, "{}: {fs} Hz", f.display());
                let t = Instant::now();
                let text = tr.transcribe(&x)?;
                println!("{:7.1} ms  {}  |  {}", t.elapsed().as_secs_f64() * 1e3, f.display(), text);
            }
            Ok(())
        }
        Cmd::Run(a) => run(a),
    }
}

/// The whisper worker (None with --no-asr).
fn spawn_asr(a: &PipeArgs) -> Result<Option<asr::AsrWorker>> {
    Ok(if a.no_asr {
        None
    } else {
        let t = Instant::now();
        let w = asr::AsrWorker::spawn(resolve_model(&a.model), !a.cpu_asr, a.language.clone())?;
        eprintln!("ASR ready ({}, {}) in {:.0} ms", a.model.display(), if a.cpu_asr { "CPU" } else { "CUDA" }, t.elapsed().as_secs_f64() * 1e3);
        Some(w)
    })
}

/// Microphones in use: all 8 minus --mics-off.
fn mics_active(a: &PipeArgs) -> Result<[bool; M]> {
    let mut active = [true; M];
    for &m in &a.mics_off {
        anyhow::ensure!((1..=M).contains(&m), "--mics-off {m}: must be 1..{M}");
        active[m - 1] = false;
    }
    Ok(active)
}

/// Everything between the health check and the frame loop, except spawning the ASR worker.
fn build_pipeline(a: &PipeArgs, active: &[bool], asr: Option<asr::AsrWorker>) -> Result<pipeline::Pipeline> {
    let active: [bool; M] = active.try_into().map_err(|_| anyhow::anyhow!("need {M} mic flags"))?;
    let m_eff = active.iter().filter(|&&x| x).count();
    anyhow::ensure!(m_eff >= 3, "only {m_eff} usable microphones");
    let exclude: Vec<(f32, f32)> = a
        .exclude
        .iter()
        .filter(|s| !s.is_empty())
        .map(|s| {
            let mut it = s.split(':');
            let az: f32 = it.next().unwrap().parse().expect("exclude az");
            let w: f32 = it.next().map(|v| v.parse().expect("exclude width")).unwrap_or(15.0);
            (az, w)
        })
        .collect();

    let geo = doa::Geometry::ring(a.radius, active);
    let wpe = wpe::Wpe::new(wpe::WpeParams { taps: a.wpe_taps.max(1), delay: a.wpe_delay, alpha: a.wpe_alpha, enabled: a.wpe_taps > 0 });
    let doa = doa::Doa::new(
        &geo,
        doa::DoaParams { f_lo: 400.0, f_hi: 4000.0, step_deg: 2.0, p_min: 0.6, coh_min: a.coh_min, decay: 0.8, peak_min: a.peak_min, max_peaks: 4, min_sep_deg: 25.0 },
    );
    let tracker = doa::Tracker::new(doa::TrackerParams {
        gate_deg: 20.0,
        confirm_hits: 3,
        tentative_ttl: ms_frames(400),
        confirmed_ttl: ms_frames(8000),
        birth_min: a.birth_min,
        max_tracks: a.max_speakers,
        az_alpha: 0.2,
        exclude,
    });
    let cfg = pipeline::PipeConfig {
        sep: sep::SepParams {
            kappa: a.kappa,
            s_bg: a.s_bg,
            cov_gamma: 0.99,
            loading: a.loading,
            solve_every: 4,
            pf_gmin: a.pf_gmin,
            sp_floor: a.sp_floor,
            act_lo: 300.0,
            act_hi: 4000.0,
            m_eff: m_eff as f32,
        },
        seg: sep::SegParams {
            on: a.vad_on,
            off: a.vad_off,
            hang_frames: ms_frames(a.hang_ms),
            pre_frames: ms_frames(150),
            min_frames: ms_frames(200),
            max_frames: ms_frames(15000),
            partial_frames: if a.partial_ms > 0 { ms_frames(a.partial_ms) } else { 0 },
        },
        doa_every: 4,
        replay_frames: ms_frames(a.replay_ms),
        ctx_frames: ms_frames(a.neural_ctx_ms),
        ref_mic: active.iter().position(|&x| x).unwrap(),
        white_load: a.white_load,
        verbose: a.verbose,
        sep_mode: a.sep,
        ghdss: ghdss::GhdssParams { eta: a.ghdss_eta, domain: a.ghdss_domain, mu: a.ghdss_mu, init_mvdr: !a.ghdss_init_ds },
    };
    let refiner = if a.neural != refine::NeuralMode::Off && asr.is_some() {
        let t = Instant::now();
        let client = refine::NeuralClient::spawn(&resolve_model(&a.neural_python), &resolve_model(&a.neural_script))?;
        eprintln!("neural separator ready in {:.0} ms (mode {:?})", t.elapsed().as_secs_f64() * 1e3, a.neural);
        let prm = refine::RefineParams { mode: a.neural, post_floor: a.neural_post_floor, loading: a.loading };
        Some(refine::Refiner::spawn(client, asr.as_ref().unwrap().tx.clone(), prm, a.verbose))
    } else {
        None
    };
    let ego = match &a.ego_noise {
        Some(d) if a.ego_mode != noise::EgoMode::Off => {
            let e = ego_covariance(d, &active, wpe::WpeParams { taps: a.wpe_taps.max(1), delay: a.wpe_delay, alpha: a.wpe_alpha, enabled: a.wpe_taps > 0 })?;
            eprintln!("ego noise: {} ({:?}, weight {} frames)", d.display(), a.ego_mode, a.ego_weight);
            Some(e)
        }
        _ => None,
    };
    let mut pipe = pipeline::Pipeline::new(cfg, geo, wpe, doa, tracker, asr);
    if a.music_refine {
        pipe.music = Some(music::MusicRefiner::new(a.music_ns, a.music_span, 16, 0.5, a.music_win));
    }
    if let Some(e) = ego {
        pipe.noise.set_ego(e, a.ego_mode, a.ego_weight);
    }
    pipe.refiner = refiner;
    Ok(pipe)
}

fn run(a: RunArgs) -> Result<()> {
    let mut chans = read_mics(&a.input)?;
    let len = chans[0].len();
    eprintln!("input: {} x {:.2} s @ {} Hz", M, len as f64 / FS as f64, FS);
    let mut active = mics_active(&a.p)?;
    if !a.no_health {
        let h = health::check(&chans, 300);
        eprintln!(
            "channel health (adjacent-pair coherence 200-2000 Hz, first 300 ms): {}",
            h.adj_msc.iter().enumerate().map(|(m, v)| format!("{}-{}:{v:.2}", m + 1, (m + 1) % M + 1)).collect::<Vec<_>>().join(" ")
        );
        for &m in &h.bad {
            eprintln!("  WARNING: mic{} is incoherent with both neighbours (dead / mis-wired / from another recording) -> excluded", m + 1);
            active[m] = false;
        }
    }
    for m in 0..M {
        if !active[m] {
            chans[m].iter_mut().for_each(|v| *v = 0.0);
        }
    }
    std::fs::create_dir_all(&a.out)?;

    let asr = spawn_asr(&a.p)?;
    let mut pipe = build_pipeline(&a.p, &active, asr)?;

    // ---- frame loop (append N zeros so the tail is flushed through the synthesis)
    let total_frames = (len + N).div_ceil(H);
    let mut hop_bufs = vec![vec![0f32; H]; M];
    let wall0 = Instant::now();
    let mut results: Vec<asr::AsrResult> = Vec::new();
    for t in 0..total_frames {
        if a.realtime {
            let due = wall0 + Duration::from_secs_f64(((t + 1) * H) as f64 / FS as f64);
            let now = Instant::now();
            if due > now {
                std::thread::sleep(due - now);
            }
        }
        for m in 0..M {
            for n in 0..H {
                let i = t * H + n;
                hop_bufs[m][n] = if i < len { chans[m][i] } else { 0.0 };
            }
        }
        let hops: Vec<&[f32]> = hop_bufs.iter().map(|v| v.as_slice()).collect();
        pipe.process(&hops);
        if let Some(w) = &pipe.asr {
            while let Ok(r) = w.rx.try_recv() {
                print_result(&r, wall0, a.realtime);
                results.push(r);
            }
        }
    }
    pipe.flush();
    let dsp_wall = wall0.elapsed();
    if let Some(r) = pipe.refiner.take() {
        r.finish();
    }
    if let Some(w) = pipe.asr.take() {
        for r in w.finish() {
            print_result(&r, wall0, a.realtime);
            results.push(r);
        }
    }

    // ---- outputs
    let mut fu = pipe.frame_us.clone();
    fu.sort_by(|a, b| a.total_cmp(b));
    let pct = |q: f64| fu[((fu.len() - 1) as f64 * q) as usize] / 1000.0;
    eprintln!(
        "\nDSP: {} frames in {:.0} ms wall ({}); per-frame ms: mean {:.2}  p50 {:.2}  p99 {:.2}  max {:.2}  (deadline 8.00)",
        fu.len(),
        dsp_wall.as_secs_f64() * 1e3,
        if a.realtime { "real-time paced".to_string() } else { format!("RTF {:.3}", dsp_wall.as_secs_f64() / (len as f64 / FS as f64)) },
        fu.iter().sum::<f32>() / fu.len() as f32 / 1000.0,
        pct(0.5),
        pct(0.99),
        pct(1.0)
    );
    write_profile(&pipe, &a.out, a.profile)?;
    write_wav(&a.out.join("wpe_mic1.wav"), &pipe.x_ref[(N - H).min(pipe.x_ref.len())..])?;
    let mut summary = Vec::new();
    for st in &pipe.streams {
        // align to the input timeline: pcm[k] is input sample origin + k
        let mut y = vec![0f32; len];
        for (k, &v) in st.pcm.iter().enumerate() {
            let i = st.origin + k as i64;
            if i >= 0 && (i as usize) < len {
                y[i as usize] = v;
            }
        }
        let name = format!("speaker{}_az{:03.0}.wav", st.id, st.az);
        write_wav(&a.out.join(&name), &y)?;
        let mut segs: Vec<&asr::AsrResult> = results.iter().filter(|r| r.speaker == st.id && !r.partial).collect();
        segs.sort_by_key(|r| r.seg);
        let text = segs.iter().map(|r| r.text.as_str()).filter(|s| !s.is_empty()).collect::<Vec<_>>().join(" ");
        summary.push(serde_json::json!({
            "speaker": st.id, "azimuth_deg": (st.az * 10.0).round() / 10.0, "wav": name,
            "detected_at_s": (st.born_frame + 1) as f64 * H as f64 / FS as f64,
            "text": text,
            "segments": segs.iter().map(|r| serde_json::json!({
                "start_s": r.t_start, "speech_end_s": r.t_speech_end, "text": r.text, "decode_ms": r.decode_ms,
            })).collect::<Vec<_>>(),
        }));
    }
    std::fs::write(a.out.join("transcript.json"), serde_json::to_string_pretty(&summary)?)?;
    if a.p.verbose {
        eprintln!("doa bins: total {} speech {} coherent {}; coherence hist {:?}", pipe.doa.stat[0], pipe.doa.stat[1], pipe.doa.stat[2], pipe.doa.coh_hist);
        for (t, pk) in &pipe.doa_log {
            eprint!("doa {:5.2}s:", (*t + 1) as f64 * H as f64 / FS as f64);
            for (az, s) in pk {
                eprint!(" {az:3.0}°({s:.3})");
            }
            eprintln!();
        }
        let mut csv = String::from("frame,time_s");
        for st in &pipe.streams {
            csv += &format!(",spk{}", st.id);
        }
        csv.push('\n');
        let first: Vec<usize> = pipe.streams.iter().map(|s| ((s.origin + N as i64) / H as i64 - 1).max(0) as usize).collect();
        for t in 0..pipe.t {
            csv += &format!("{t},{:.3}", (t + 1) as f64 * H as f64 / FS as f64);
            for (si, log) in pipe.act_log.iter().enumerate() {
                let v = if t >= first[si] { log.get(t - first[si]).copied().unwrap_or(0.0) } else { 0.0 };
                csv += &format!(",{v:.4}");
            }
            csv.push('\n');
        }
        std::fs::write(a.out.join("activity.csv"), csv)?;
    }

    println!("\n==== per-speaker transcripts ====");
    for s in &summary {
        println!("speaker {} @ {:>5.1}°  (detected {:.2} s): {}", s["speaker"], s["azimuth_deg"].as_f64().unwrap(), s["detected_at_s"].as_f64().unwrap(), s["text"].as_str().unwrap());
    }
    println!("outputs in {}", a.out.display());
    Ok(())
}

fn print_result(r: &asr::AsrResult, wall0: Instant, realtime: bool) {
    let kind = if r.partial { "partial" } else if r.hyp.accepted { "FINAL" } else { "reject" };
    let shown = if r.hyp.accepted { r.text.clone() } else { format!("({}) [no_speech {:.2}, p {:.2}]", r.hyp.text, r.hyp.no_speech, r.hyp.avg_p) };
    if realtime {
        // audio clock == wall clock since start, so this is true end-of-speech -> text latency
        let done = (r.done - wall0).as_secs_f64();
        println!(
            "[{kind:7}] spk{} seg{} {:.2}-{:.2}s  latency after speech end {:4.0} ms (neural {:3.0} ms, decode {:3.0} ms): {}",
            r.speaker,
            r.seg,
            r.t_start,
            r.t_speech_end,
            (done - r.t_speech_end) * 1e3,
            r.refine_ms,
            r.decode_ms,
            shown
        );
    } else {
        println!("[{kind:7}] spk{} seg{} {:.2}-{:.2}s  neural {:3.0} ms, decode {:3.0} ms: {}", r.speaker, r.seg, r.t_start, r.t_speech_end, r.refine_ms, r.decode_ms, shown);
    }
}
