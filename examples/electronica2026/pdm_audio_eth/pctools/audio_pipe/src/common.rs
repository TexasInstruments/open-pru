// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
//! Shared constants, types, WAV I/O and small 8x8 complex linear algebra.
use anyhow::{Context, Result, bail};
use num_complex::{Complex32, Complex64};
use std::path::Path;

pub const FS: usize = 16_000;
pub const N: usize = 512; // STFT window (32 ms)
pub const H: usize = 128; // hop (8 ms, 75 % overlap)
pub const F: usize = N / 2 + 1; // 257 bins
pub const M: usize = 8; // microphones
pub const SPEED_OF_SOUND: f32 = 343.0;

pub type C = Complex32;
pub type Vec8 = [C; M];
pub type Mat8 = [[C; M]; M];
/// One STFT frame, bin-major: `frame[f][m]`.
pub type Frame = Vec<Vec8>;

pub const ZERO: C = C { re: 0.0, im: 0.0 };
pub const ZV: Vec8 = [ZERO; M];
pub const ZM: Mat8 = [[ZERO; M]; M];

pub fn bin_hz(f: usize) -> f32 {
    f as f32 * FS as f32 / N as f32
}

/// Read mic1..mic8.wav from `dir`. Returns channels of f32 in [-1, 1].
pub fn read_mics(dir: &Path) -> Result<Vec<Vec<f32>>> {
    let mut chans = Vec::with_capacity(M);
    for m in 1..=M {
        let p = dir.join(format!("mic{m}.wav"));
        let (x, fs) = read_wav(&p)?;
        if fs != FS as u32 {
            bail!("{}: sample rate {fs}, expected {FS}", p.display());
        }
        chans.push(x);
    }
    let len = chans.iter().map(|c| c.len()).min().unwrap();
    for c in chans.iter_mut() {
        c.truncate(len);
    }
    Ok(chans)
}

pub fn read_wav(p: &Path) -> Result<(Vec<f32>, u32)> {
    let mut r = hound::WavReader::open(p).with_context(|| format!("open {}", p.display()))?;
    let spec = r.spec();
    let ch = spec.channels as usize;
    let all: Vec<f32> = match spec.sample_format {
        hound::SampleFormat::Float => r.samples::<f32>().collect::<Result<_, _>>()?,
        hound::SampleFormat::Int => {
            let scale = 1.0 / (1u64 << (spec.bits_per_sample - 1)) as f32;
            r.samples::<i32>().map(|s| s.map(|v| v as f32 * scale)).collect::<Result<_, _>>()?
        }
    };
    // take the first channel if the file is multichannel
    Ok((all.chunks(ch).map(|c| c[0]).collect(), spec.sample_rate))
}

pub fn write_wav(p: &Path, x: &[f32]) -> Result<()> {
    let spec = hound::WavSpec { channels: 1, sample_rate: FS as u32, bits_per_sample: 16, sample_format: hound::SampleFormat::Int };
    let mut w = hound::WavWriter::create(p, spec)?;
    for &v in x {
        w.write_sample((v.clamp(-1.0, 1.0) * 32767.0).round() as i16)?;
    }
    w.finalize()?;
    Ok(())
}

// ------------------------------------------------------------------ 8x8 helpers

#[inline]
pub fn rank1_acc(a: &mut Mat8, x: &Vec8, wgt: f32) {
    for i in 0..M {
        let xi = x[i] * wgt;
        for j in 0..M {
            a[i][j] += xi * x[j].conj();
        }
    }
}

#[inline]
pub fn scale_mat(a: &mut Mat8, s: f32) {
    for r in a.iter_mut() {
        for v in r.iter_mut() {
            *v *= s;
        }
    }
}

pub fn trace_re(a: &Mat8) -> f32 {
    (0..M).map(|i| a[i][i].re).sum()
}

/// x^H A x (real part; A Hermitian).
pub fn quad(a: &Mat8, x: &Vec8) -> f32 {
    let mut s = ZERO;
    for i in 0..M {
        let mut r = ZERO;
        for j in 0..M {
            r += a[i][j] * x[j];
        }
        s += x[i].conj() * r;
    }
    s.re
}

/// a^H x
#[inline]
pub fn dot_h(a: &Vec8, x: &Vec8) -> C {
    let mut s = ZERO;
    for m in 0..M {
        s += a[m].conj() * x[m];
    }
    s
}

/// Solve (A + load*I) x = b for Hermitian positive-definite A, via f64 Cholesky.
/// Returns None if the matrix is not positive definite even after loading.
pub fn herm_solve(a: &Mat8, load: f32, b: &Vec8) -> Option<Vec8> {
    let mut l = [[Complex64::new(0.0, 0.0); M]; M];
    for i in 0..M {
        for j in 0..=i {
            let mut s = Complex64::new(a[i][j].re as f64, a[i][j].im as f64);
            if i == j {
                s.re += load as f64;
            }
            for k in 0..j {
                s -= l[i][k] * l[j][k].conj();
            }
            if i == j {
                if s.re <= 0.0 {
                    return None;
                }
                l[i][i] = Complex64::new(s.re.sqrt(), 0.0);
            } else {
                l[i][j] = s / l[j][j].re;
            }
        }
    }
    // forward: L y = b
    let mut y = [Complex64::new(0.0, 0.0); M];
    for i in 0..M {
        let mut s = Complex64::new(b[i].re as f64, b[i].im as f64);
        for k in 0..i {
            s -= l[i][k] * y[k];
        }
        y[i] = s / l[i][i].re;
    }
    // backward: L^H x = y
    let mut x = [Complex64::new(0.0, 0.0); M];
    for i in (0..M).rev() {
        let mut s = y[i];
        for k in i + 1..M {
            s -= l[k][i].conj() * x[k];
        }
        x[i] = s / l[i][i].re;
    }
    let mut out = ZV;
    for i in 0..M {
        out[i] = C::new(x[i].re as f32, x[i].im as f32);
    }
    Some(out)
}

/// Lower Cholesky factor of (A + load*I), f32. None if not positive definite.
pub fn chol(a: &Mat8, load: f32) -> Option<Mat8> {
    let mut l = ZM;
    for i in 0..M {
        for j in 0..=i {
            let mut s = a[i][j];
            if i == j {
                s.re += load;
            }
            for k in 0..j {
                s -= l[i][k] * l[j][k].conj();
            }
            if i == j {
                if !(s.re > 0.0) {
                    return None;
                }
                l[i][i] = C::new(s.re.sqrt(), 0.0);
            } else {
                l[i][j] = s / l[j][j].re;
            }
        }
    }
    Some(l)
}

/// Solve L y = b (L lower-triangular from `chol`): whitening y = L⁻¹ b.
#[inline]
pub fn fwd_sub(l: &Mat8, b: &Vec8) -> Vec8 {
    let mut y = ZV;
    for i in 0..M {
        let mut s = b[i];
        for k in 0..i {
            s -= l[i][k] * y[k];
        }
        y[i] = s / l[i][i].re;
    }
    y
}

pub fn wrap_deg(a: f32) -> f32 {
    a.rem_euclid(360.0)
}

/// Signed circular difference a - b in (-180, 180].
pub fn ang_diff(a: f32, b: f32) -> f32 {
    let d = (a - b).rem_euclid(360.0);
    if d > 180.0 { d - 360.0 } else { d }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn solve_identity_and_random() {
        let mut a = ZM;
        // A = B B^H + I, well conditioned
        let b: Vec<Vec8> = (0..4)
            .map(|k| core::array::from_fn(|m| C::new(((m * 7 + k * 3) % 5) as f32 - 2.0, ((m + 2 * k) % 3) as f32 - 1.0)))
            .collect();
        for v in &b {
            rank1_acc(&mut a, v, 1.0);
        }
        let rhs: Vec8 = core::array::from_fn(|m| C::new(m as f32, 1.0));
        let x = herm_solve(&a, 1.0, &rhs).unwrap();
        // check (A + I) x == rhs
        for i in 0..M {
            let mut s = x[i];
            for j in 0..M {
                s += a[i][j] * x[j];
            }
            assert!((s - rhs[i]).norm() < 1e-3, "row {i}: {s} vs {}", rhs[i]);
        }
    }
    #[test]
    fn angles() {
        assert!((ang_diff(10.0, 350.0) - 20.0).abs() < 1e-4);
        assert!((ang_diff(350.0, 10.0) + 20.0).abs() < 1e-4);
    }
}

/// Heap bytes held by a slice's elements (for the memory profile).
pub fn vbytes<T>(v: &[T]) -> usize {
    std::mem::size_of_val(v)
}
