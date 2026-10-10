#!/usr/bin/env python3
"""Fixups from pristine samples: Samples_src/*.wav -> Samples_src/loops/*.wav.

All DSP math runs on numpy (no hand-rolled resamplers/autocorrelations).
Fixups are derived ONLY from the pristine WAVs, so re-running the pipeline is
idempotent: shift/saturation can never compound across passes.

Rules (tunable at the top of this file): REGISTER_SHIFT (-12/0/+12 octave
resample for SHIFT_VOICES), optional tanh saturation (SATURATE set, OFF by
default), attack-skip a0=10%, longest-first seam search with cost<=0.30,
minimum 4 pitch periods AND >=125 ms, micro-crossfade, honest SKIP reports
for noise-like or unloopable sources rather than mangling them.

Run from the repo root (gen_all.sh does this for you):
    python tools/loops.py
"""
import wave

import numpy as np

import samplelib

RATE = samplelib.RATE
DECIM = 4

# Register shift: applied to SHIFT_VOICES, one knob for the whole build.
#   +12 = 2x linear UPSAMPLE (file plays an octave lower at ratio 1; demo
#         registers pair an octave down) - the shipped state on the 10 mm
#         keychain driver, where it killed aliasing harshness.
#    0  = raw pristine pitch.
#   -12 = read at 2x (file halves in length; plays an octave HIGHER at
#         ratio 1) - useful on a big driver if registers go too low.
# Pair with the demo octave registers in Tracker.cpp: changing this number
# without moving them transposes those voices an octave.
REGISTER_SHIFT = 12

# Voices that receive REGISTER_SHIFT. Percussion-voiced entries keep their
# register in the shipped config and are listed out.
SHIFT_VOICES = {
    "bass1", "synth2", "pad1", "jpad1", "pad3", "jlead2",
    "jlead3", "jlead4", "guitar1", "pad2", "synth1", "synth3", "bass2",
}

# Tier-2 speaker compensation: tanh soft saturation generating 2nd-4th
# harmonics for drivers that cannot radiate the fundamental (the 10 mm /
# Fs ~900 Hz keychain unit). DEFAULT OFF: a proper speaker (e.g. the
# 55 mm) radiates clean fundamentals, and saturation would just add fuzz.
# Re-enable per voice by listing names here; applied BEFORE seam search so
# loop seams match the saturated waveform. Unity at peak; loudness is
# renormalized afterward by the tier-1 gains.
SATURATE = set()
# Drive knob: 2.0 = gentle (keeps timbre), 3-4 = overtone-heavy "fuzz" that
# makes sub-band fundamentals audible. Loudness target/band edges live in
# samplelib.py TUNABLES. Re-run gen_all.sh after changing.
SAT_DRIVE = 2.0


def read_wav_array(path):
    try:
        with wave.open(str(path), "rb") as wavFile:
            if (wavFile.getnchannels() != 1 or wavFile.getsampwidth() != 2
                    or wavFile.getframerate() != RATE):
                return None
            buf = wavFile.readframes(wavFile.getnframes())
    except Exception:
        return None
    return np.frombuffer(buf, dtype="<i2").astype(np.int64)


def register_shift(samples, semitones):
    """Octave resample: +12 upsample (2x length), -12 read at 2x, 0 raw."""
    length = len(samples)
    if semitones > 0:
        out_len, step = 2 * length, 0.5
    else:
        out_len, step = length // 2, 2.0
    pos = np.arange(out_len) * step
    idx = np.arange(length, dtype=np.float64)
    return np.rint(np.interp(pos, idx, samples.astype(np.float64))).astype(np.int64)


def saturate(samples, drive=SAT_DRIVE):
    peak = int(np.max(np.abs(samples))) if samples.size else 0
    if peak < 64:  # silence guard
        return samples
    peak_float = float(peak)
    return np.rint(
        peak_float / np.tanh(drive) * np.tanh(drive * samples / peak_float)
    ).astype(np.int64)


def best_period(samples, a0):
    """Normalized autocorrelation on the decimated sustain window."""
    seg = samples[a0::DECIM].astype(np.float64)
    length = len(seg)
    if length < 600:
        return 0, 0.0
    sq = seg * seg
    energy0 = sq.sum() or 1.0
    max_lag = min(300, length // 3)
    lags = np.arange(12, max_lag)
    if len(lags) == 0:
        return 0, 0.0
    acf = np.correlate(seg, seg, mode="full")[length - 1:]  # acf[lag] = sum seg[i]*seg[i+lag]
    prefix = np.concatenate(([0.0], np.cumsum(sq)))    # et[lag] = sum seg[lag:]^2
    num = acf[12:max_lag]
    et = np.maximum(energy0 - prefix[12:max_lag], 1.0)
    score = num / np.sqrt(energy0 * et)
    bestIndex = int(np.argmax(score))  # first max, matching the C++ strict >
    return int(lags[bestIndex]) * DECIM, float(score[bestIndex])


def prep(name):
    src = samplelib.SRC_DIR / f"{name}.wav"
    samples = read_wav_array(src)
    if samples is None:
        print(f"{name:<10s} MISSING pristine wav ({src})")
        return
    shifted = name in SHIFT_VOICES and REGISTER_SHIFT != 0
    sat = bool(SATURATE) and name in SATURATE
    if shifted:
        samples = register_shift(samples, REGISTER_SHIFT)
    if sat:
        samples = saturate(samples)
    length = len(samples)
    if length < RATE // 8:
        print(f"{name:<10s} SKIP(too short {length})")
        return
    a0 = length // 10
    period, acf = best_period(samples, a0)
    if period == 0 or acf < 0.5:
        print(f"{name:<10s} SKIP(noise-like acf={acf:.2f})")
        return
    avail = length - a0
    min_l = max(4 * period, RATE // 8)
    if avail < min_l:
        print(f"{name:<10s} SKIP(short sustain avail={avail} P={period})")
        return
    sustain = samples[a0:min(length, a0 + RATE // 2)]
    rms = np.sqrt(np.mean(sustain.astype(np.float64) ** 2)) + 1.0
    inc = samples[a0 + 1] - samples[a0]

    # Longest-first: the largest L whose seam cost passes is the winner.
    ls = np.arange(min_l, avail - 1)  # L valid while a0+L+1 < n
    end = a0 + ls
    cost = (np.abs(samples[end] - samples[a0])
            + np.abs((samples[end] - samples[end - 1]) - inc)) / rms
    ok = np.flatnonzero(cost <= 0.30)
    if len(ok) == 0:
        print(f"{name:<10s} SKIP(no seam <= 0.30 in {avail - min_l} candidates, P={period})")
        return
    loopLength = int(ls[ok[-1]])  # descending search order -> largest valid L
    loop = samples[a0:a0 + loopLength].copy()

    cf = max(4, min(24, loopLength // 16))
    tail = loop[-cf:].copy()
    weight = (np.arange(cf) + 1.0) / cf
    loop[loopLength - cf:] = np.rint(
        (1 - weight) * tail + weight * loop[:cf]
    ).astype(np.int64)

    out = samplelib.LOOP_DIR / f"{name}.wav"
    with wave.open(str(out), "wb") as wavOut:
        wavOut.setnchannels(1)
        wavOut.setsampwidth(2)
        wavOut.setframerate(RATE)
        wavOut.writeframes(loop.astype("<i2").tobytes())
    print(f"{name:<10s} OK{f' {REGISTER_SHIFT:+d}' if shifted else ''}"
          f"{' SAT' if sat else ''}"
          f" P={period} loop={loopLength} ({loopLength / RATE:.2f}s) "
          f"cost={cost[ok[-1]]:.3f} acf={acf:.2f}")


def main():
    samplelib.LOOP_DIR.mkdir(parents=True, exist_ok=True)
    for name in samplelib.MELODIC:
        prep(name)


if __name__ == "__main__":
    main()
