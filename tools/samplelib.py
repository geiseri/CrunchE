"""Header/gain packaging for the CrunchE sample pipeline.

WAV creation (source export) and loop prep live in make_samples.cpp, which
includes the SAME Samples/*.h arrays the firmware does - single parser,
byte-verifyable. This module only: reads Samples_src(+loops) WAVs and
writes Samples/*.h headers plus the generated SampleGains.h.

Layout:
  Samples_src/*.wav        human-owned sources of truth (mono 16-bit 22050)
  Samples_src/loops/*.wav  C++ loop-prep output (phase-aligned seams)
  Samples/*.h              generated build artifacts
  SampleGains.h            generated RMS-normalized gain constants
"""
import ast
import math
import pathlib
import re
import wave

import numpy as np
from scipy import signal

RATE = 22050  # MIRROR of AudioConfig.h kAudioSampleRate - change together
DECIM = 4

# ----------------------------------------------------------------- TUNABLES
# Speaker loudness model. Full documentation - knob meanings, reference
# profiles by Fs/rating (0.5 W keychain through 1 W 55 mm), and the swap
# checklist - lives in README.md, section "Speaker Profiles (loudness-model
# tunables)". After any change here:  sh tools/gen_all.sh  (gates prove it).
#
# Current target: 55 mm / 8 ohm / 1 W full-range driver (Fs ~60-90 Hz).
# History: 10 mm keychain unit (Fs ~900 Hz) used (600, 6000).
SPEAKER_BAND_HZ = (250, 8000.0)
BAND_ORDER = 2                # Butterworth order per band edge
TARGET_WRMS = 3400.0          # in-band loudness target per voice
PEAK_CAP = 9000.0             # raw-peak ceiling (hardware headroom bound)
# Saturation drive for tier-2 harmonic generation lives in loops.py
# (SATURATE set + SAT_DRIVE) - raise it when a voice is peak-capped but
# still quiet IN-BAND (fundamental below SPEAKER_BAND_HZ[0]).

MELODIC = [
    "bass1", "jbass2", "pad1", "jpad1", "pad3", "bongo1", "synth2",
    "jbass1", "jlead1", "jlead2", "bass2", "guitar1", "jlead3", "jlead4",
    "kick3", "pad2", "snareB3", "synth1", "synth3",
]
DRUMS = [
    "kick1", "kick2", "snare1", "snare2", "snare3", "snareB1", "snareB2",
    "hihat1", "hihat2", "clap1", "crash1", "ride1",
]
SFX = [f"sfx{index}" for index in range(1, 13)]
EXTRA_TABLES = ["pureSin", "pureTriSoft"]
ALL = MELODIC + DRUMS + SFX + EXTRA_TABLES

SRC_DIR = pathlib.Path("Samples_src")
LOOP_DIR = SRC_DIR / "loops"
HDR_DIR = pathlib.Path("Samples")


def load_header(path):
    txt = path.read_text()
    match = re.search(r"(?:const )?int (\w+)\[\] = \{(.*?)\};", txt, re.S)
    name = match.group(1)
    body = match.group(2).strip().rstrip(",")
    # ast.literal_eval is the stdlib's EXACT number parser - unlike regex
    # splitting it can never mangle signs or whitespace in the array body.
    vals = list(ast.literal_eval("[" + body + "]"))
    guard = re.search(r"#ifndef (\w+)", txt).group(1)
    return name, guard, vals

def write_header(path, name, guard, vals):
    body = ",\n".join(
        ",".join(str(sampleValue) for sampleValue in vals[index:index + 12])
        for index in range(0, len(vals), 12)
    )
    path.write_text(
        f"#ifndef {guard}\n#define {guard}\nconst int {name}[] = {{\n{body},\n}};\n"
        f"int {name}Length = {len(vals)};\n#endif\n"
    )


def read_wav(path):
    with wave.open(str(path), "rb") as wavFile:
        if wavFile.getnchannels() != 1 or wavFile.getsampwidth() != 2:
            raise ValueError(f"{path}: must be mono 16-bit")
        if wavFile.getframerate() != RATE:
            raise ValueError(f"{path}: must be {RATE} Hz (got {wavFile.getframerate()})")
        raw = wavFile.readframes(wavFile.getnframes())
    return list(memoryview(raw).cast("h"))


def stats(vals):
    """Tier-1 gains: normalize RADIATED-BAND rms, not flat rms.

    Flat RMS lets voices putting energy outside the speaker's passband
    (sub-bass fundamentals, HF fizz) sit too quiet while masking what is
    actually heard. The metric is the band the driver can radiate; the
    peak cap still bounds hardware headroom.
    """
    samples = np.asarray(vals, dtype=np.float64)
    if samples.size == 0:
        return 0, 0.0, 0.0, 1.0
    peak = int(np.max(np.abs(samples)))
    rms = float(np.sqrt(np.mean(samples * samples)))
    wrms = float(np.sqrt(np.mean(weighted(samples) ** 2)))
    gain = float(min(TARGET_WRMS / (wrms if wrms > 1 else 1.0),
                     (PEAK_CAP / peak) if peak else 1.0))
    return peak, rms, wrms, gain


# ---------------------------------------------------------------- tier 1
# Loudness metric = what the CURRENT speaker can actually radiate, per
# SPEAKER_BAND_HZ. Energy outside the band only loads the master limiter
# (or is inaudible), so normalizing IN-band is the honest perceived-loudness
# target; 2nd-order Butterworth edges model the skirts. scipy-designed once.

_BAND_SOS_CACHE = {}


def _band_sos():
    sos = _BAND_SOS_CACHE.get(RATE)
    if sos is None:
        lo, hi = SPEAKER_BAND_HZ
        hp = signal.butter(BAND_ORDER, lo, btype="highpass", fs=RATE, output="sos")
        lp = signal.butter(BAND_ORDER, hi, btype="lowpass", fs=RATE, output="sos")
        sos = np.vstack([hp, lp])
        _BAND_SOS_CACHE[RATE] = sos
    return sos


def weighted(vals):
    samples = np.asarray(vals, dtype=np.float64)
    if samples.size < 4:
        return samples
    return signal.sosfilt(_band_sos(), samples)


def rms_of(samples):
    samples = np.asarray(samples, dtype=np.float64)
    if samples.size == 0:
        return 0.0
    return float(np.sqrt(np.mean(samples * samples)))
