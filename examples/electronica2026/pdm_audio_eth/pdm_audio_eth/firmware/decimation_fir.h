/* Front-end decimation filters for IM73D122 @ OSR 192 (fin=3.072 MHz)
   Two options; pick per your anti-alias needs.

   OPTION A - merged one-stage FIR (comb^3 * droop-comp), 16 kHz.
     Applied to the DECIMATED 3rd-integrator (accumulator) sample stream.
     Same anti-alias as plain SINC3 (rejection unchanged) - this only
     collapses CIC-comb + droop-comp into ONE FIR. Normalise output by
     1 / R^N = 1/7077888 (CIC3 DC gain). Prefer float on an FPU core;
     its own DC gain is 0 (it is meaningful only on integrator output). */
#define MERGED_TAPS 44
#define CIC3_NORM   (1.0f/7077888.0f)
static const float merged_comb3_comp_f32[MERGED_TAPS] = {
    -0.001886f,  0.007728f, -0.013366f,  0.012518f,
    -0.003632f, -0.012933f,  0.035931f, -0.061199f,
     0.081395f, -0.086407f,  0.064524f, -0.004323f,
    -0.102896f,  0.260417f, -0.463389f,  0.696014f,
    -0.927645f,  1.103770f, -1.115983f,  0.673035f,
     1.495701f, -4.733172f,  4.733172f, -1.495701f,
    -0.673035f,  1.115983f, -1.103770f,  0.927645f,
    -0.696014f,  0.463389f, -0.260417f,  0.102896f,
     0.004323f, -0.064524f,  0.086407f, -0.081395f,
     0.061199f, -0.035931f,  0.012933f,  0.003632f,
    -0.012518f,  0.013366f, -0.007728f,  0.001886f,
};

   /* ---------------------------------------------------------------- */
/* OPTION B - sharpened two-stage: 192 = 96 x 2. Better alias rejection.
   STAGE 1: CIC3 decimate by 96 -> 32 kHz (your accumulators; read 3rd
            integrator every 96, apply comb^3 = z-domain (1-z^-1)^3,
            normalise by 96^3). Long moving-sums may be formed as
            I1[n]-I1[n-L] from the 1st accumulator (running-sum trick).
   STAGE 2: this sharp FIR, decimate by 2 -> 16 kHz. Flattens stage-1
            droop AND stops 9-16 kHz to kill the near-band fold. */
#define STAGE2_TAPS 63
static const float stage2_fir_f32[STAGE2_TAPS] = {
     0.00005872f,  0.00027631f,  0.00010001f, -0.00065134f,
    -0.00050221f,  0.00114741f,  0.00128193f, -0.00166228f,
    -0.00248940f,  0.00202360f,  0.00376951f, -0.00294804f,
    -0.00611597f,  0.00356636f,  0.00894969f, -0.00443268f,
    -0.01282171f,  0.00552732f,  0.01831215f, -0.00679788f,
    -0.02687083f,  0.00600311f,  0.03633063f, -0.00658857f,
    -0.05232919f,  0.00525812f,  0.07878844f, -0.00038083f,
    -0.13504418f, -0.02443495f,  0.33907754f,  0.54920930f,
     0.33907754f, -0.02443495f, -0.13504418f, -0.00038083f,
     0.07878844f,  0.00525812f, -0.05232919f, -0.00658857f,
     0.03633063f,  0.00600311f, -0.02687083f, -0.00679788f,
     0.01831215f,  0.00552732f, -0.01282171f, -0.00443268f,
     0.00894969f,  0.00356636f, -0.00611597f, -0.00294804f,
     0.00376951f,  0.00202360f, -0.00248940f, -0.00166228f,
     0.00128193f,  0.00114741f, -0.00050221f, -0.00065134f,
     0.00010001f,  0.00027631f,  0.00005872f,
};
static const q15_t stage2_fir_q15[STAGE2_TAPS] = {
         2,      9,      3,    -21,    -16,     38,     42,    -54,
       -82,     66,    124,    -97,   -200,    117,    293,   -145,
      -420,    181,    600,   -223,   -881,    197,   1190,   -216,
     -1715,    172,   2582,    -12,  -4425,   -801,  11111,  17996,
     11111,   -801,  -4425,    -12,   2582,    172,  -1715,   -216,
      1190,    197,   -881,   -223,    600,    181,   -420,   -145,
       293,    117,   -200,    -97,    124,     66,    -82,    -54,
        42,     38,    -16,    -21,      3,      9,      2,
};

/* ================================================================
   SELF-CONTAINED: full-scale anchor, CIC-gain scaling, DC blocker,
   24-bit PCM conversion.   Requires <stdint.h> and <math.h>.
   ================================================================ */

/* ---- Full-scale anchor -----------------------------------------
   IM73D122: sensitivity -26 dBFS  =>  0 dBFS = 120 dBSPL.
   Decimator natural full scale IS signed 24-bit two's complement:
     PDM all-1s -> +0x7FFFFF (+8388607),  all-0s -> -0x800000.
   Usable acoustic range ~99 dB (21 dBSPL floor -> 0 dBFS) = ~16-17
   real bits; the low ~7 bits are self-noise (they self-dither). */
#define PCM24_MAX   ( 8388607)          /* +2^23 - 1 */
#define PCM24_MIN   (-8388608)          /* -2^23     */

/* ---- CIC(stage-1) gain folded into ONE output scale ------------
   comb^3 output carries the CIC DC gain R1^N = 96^3 = 884736.
   Fold it into a single multiply on the stage-2 FIR output so the
   sample emerges already referenced to full scale (no divide then
   re-multiply, no wasted precision). 192^3 = 7077888 if 1-stage. */
#define CIC1_GAIN   (884736.0f)
#define CIC1_NORM   (1.0f/884736.0f)

/* ---- DC blocker, float direct form -----------------------------
   H(z) = (1 - z^-1)/(1 - a z^-1),  a = 1 - 2^-7 = 0.9921875
   -> fc ~= 19.9 Hz @ 16 kHz.  One dcb_t per channel. */
#define DCB_A_F   0.9921875f
typedef struct { float x1, y1; } dcb_t;
static inline float dc_block_f(dcb_t *s, float x){
    float y = x - s->x1 + DCB_A_F * s->y1;
    s->x1 = x; s->y1 = y;
    return y;
}

/* ---- DC blocker, fixed-point SHIFT form (no dead-band) ---------
   DC estimate held in Q32 inside int64, so the leak is never
   truncated to zero for small offsets (a plain integer >>K would
   ignore any offset below 2^K LSB).  K=7 -> a = 1 - 2^-7. */
#define DCB_K   7
typedef struct { int64_t dc; } dcb_i_t;
static inline int32_t dc_block_i(dcb_i_t *s, int32_t x /* 24-bit in int32 */){
    int32_t d = (int32_t)(s->dc >> 32);               /* DC estimate     */
    s->dc += (((int64_t)x << 32) - s->dc) >> DCB_K;   /* leak toward x   */
    return x - d;                                     /* highpass output */
}

/* ---- Quantize to signed 24-bit PCM -----------------------------
   float path: chain normalised so +-1.0 == 0 dBFS. */
static inline int32_t to_pcm24_f(float y){
    int32_t s = (int32_t)lrintf(y * 8388608.0f);      /* 2^23 */
    if (s > PCM24_MAX) s = PCM24_MAX;
    if (s < PCM24_MIN) s = PCM24_MIN;
    return s;                                          /* right-justified, sign-extended */
}
/* fixed path: keep FIR accumulator in int64, apply the folded
   scale, arithmetic-shift down into 24-bit, then clip. */
static inline int32_t to_pcm24_i(int64_t acc, int shift){
    int32_t s = (int32_t)(acc >> shift);
    if (s > PCM24_MAX) s = PCM24_MAX;
    if (s < PCM24_MIN) s = PCM24_MIN;
    return s;
}

/* ---- Packing (sink-dependent) ----------------------------------
   DSP/STFT : keep int32 as-is (right-justified, the value IS sample)
   I2S/TDM  : MSB-align -> (s << 8) in a 32-bit slot
   WAV LE   : 3 bytes little-endian below. */
static inline void pack_pcm24_le(int32_t s, unsigned char *b){
    b[0]=(unsigned char)(s);
    b[1]=(unsigned char)(s>>8);
    b[2]=(unsigned char)(s>>16);
}

/* Per-channel float chain:
     comb^3(decimated acc) -> arm_fir_f32(stage2)*CIC1_NORM ->
     dc_block_f -> to_pcm24_f .
   No dither: the ~21 dBA self-noise spans hundreds of 24-bit LSBs
   and already dithers the quantization. */
