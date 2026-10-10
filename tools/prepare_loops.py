#!/usr/bin/env python3
"""Offline loop preparation for CrunchE melodic samples.

The runtime wraps sampleIndex at file length; recordings end at arbitrary
amplitude/phase, so sustained voices click once per wrap (~9x/second at
demo read ratios) - heard as buzz. A runtime crossfade comb-filtered
against itself (shrill), so the seam is fixed HERE, with the whole file in
view:

  1. estimate the dominant pitch period via normalized autocorrelation of
     the sustain window; low periodicity = noise-like = skipped honestly;
  2. skip the attack and choose a loop length of whole periods whose end
     has the best amplitude+slope match to the start (phase-aligned seam);
  3. crossfade only the final few samples into the head (safe because the
     segments are already phase-aligned - no comb);
  4. rewrite the header in place; originals are kept under
     Samples/pre_loop_backup/ and git tracks the old versions too.

After running this, re-run the native harness/stacks/sweeps; source gains
are peak/rms-normalized in the engine table and the trim changes them -
the harness voice-level lines will show any loudness drift. The harness's
clicks/s metric is the acceptance check for this tool.
"""
import math
import pathlib
import re
import sys

RATE = 22050
DECIM = 4
MELODIC = [
    "bass1", "jbass2", "pad1", "jpad1", "pad3", "bongo1", "synth2",
    "jbass1", "jlead1", "jlead2", "bass2", "guitar1", "jlead3", "jlead4",
    "kick3", "pad2", "snareB3", "synth1", "synth3",
]


def load_array(path):
    txt = path.read_text()
    match = re.search(r"const int (\w+)\[\] = \{(.*)\};", txt, re.S)
    body = match.group(2).split("}")[0]
    vals = [int(sampleValue) for sampleValue in re.split(r"[^\d-]+", body)
            if sampleValue and sampleValue != "-"]
    guard = re.search(r"#ifndef (\w+)", txt).group(1)
    return vals, guard


def store_array(path, name, guard, vals):
    body = ",\n".join(
        ",".join(str(sampleValue) for sampleValue in vals[index:index + 12])
        for index in range(0, len(vals), 12)
    )
    path.write_text(
        f"#ifndef {guard}\n#define {guard}\nconst int {name}[] = {{\n{body},\n}};\n"
        f"int {name}Length = {len(vals)};\n#endif\n"
    )


def best_period(seg):
    """Normalized autocorrelation on the decimated sustain window."""
    length = len(seg)
    energy0 = sum(sampleValue * sampleValue for sampleValue in seg) or 1
    best_score, best_lag = 0.0, 0
    for lag in range(12, min(300, length // 3)):  # ~18 Hz .. 460 Hz at DECIM=4
        num = sum(seg[index] * seg[index + lag] for index in range(length - lag))
        etail = sum(sampleValue * sampleValue for sampleValue in seg[lag:]) or 1
        score = num / math.sqrt(energy0 * etail)
        if score > best_score:
            best_score, best_lag = score, lag
    return best_lag * DECIM, best_score


def process(name, dry):
   
    path = pathlib.Path(f"Samples/{name}.h")
    data, guard = load_array(path)
    length = len(data)
    # CrunchE samples are short by design (0.12-0.4 s); the practical floor
    # is "at least ~4 pitch periods of sustain", checked after detection.
    if length < RATE // 12:  # < ~83 ms: nothing to loop
        return name, f"SKIP(short {length})"

    a0 = int(length * 0.10)
    seg = data[a0:][::DECIM]
    period, score = best_period(seg)
    if period == 0 or score < 0.5:
        return name, f"SKIP(noise-like {length}smp acf={score:.2f})"

    avail = length - a0
    if avail < 4 * period:
        return name, f"SKIP(too few periods {avail}smp P={period})"

    # Scan every candidate loop end in the sustain region (not a window
    # around a target length): seam smoothness is what matters, and the
    # best endpoint can sit anywhere the content repeats. Ties prefer
    # longer loops (more character retained).
    inc = data[a0 + 1] - data[a0]
    half = seg[: len(seg) // 2]
    rms_scale = max(
        math.sqrt(sum(sampleValue * sampleValue for sampleValue in half)
                  / max(len(half), 1)), 1.0
    )
    best_cost, best_L = None, 0
    for loopLength in range(4 * period, avail):
        if a0 + loopLength + 1 >= length:
            break
        cost = (abs(data[a0 + loopLength] - data[a0])
                + abs(data[a0 + loopLength] - data[a0 + loopLength - 1] - inc)) / rms_scale
        if best_cost is None or cost < best_cost * 0.9:
            best_cost, best_L = cost, loopLength

    if best_cost is None or best_cost > 0.30:
        return name, f"SKIP(no smooth seam best cost={best_cost})"

    loop = data[a0:a0 + best_L]
    CF = max(4, min(24, best_L // 16))
    tail = list(loop[-CF:])
    for fadeIndex in range(CF):
        weight = (fadeIndex + 1) / CF
        loop[len(loop) - CF + fadeIndex] = int(round(
            (1 - weight) * tail[fadeIndex] + weight * loop[fadeIndex]))

    if not dry:
        bak = pathlib.Path("Samples/pre_loop_backup")
        bak.mkdir(exist_ok=True)
        if not (bak / f"{name}.h").exists():
            (bak / f"{name}.h").write_text(path.read_text())
        store_array(path, name, guard, loop)
    return name, (
        f"OK P={period} loop={best_L} ({best_L / RATE:.2f}s) "
        f"cost={best_cost:.3f} acf={score:.2f}"
    )


def main():
    args = [arg for arg in sys.argv[1:] if not arg.startswith("--")]
    dry = "--dry-run" in sys.argv
    for name in (args or MELODIC):
        result = process(name, dry)
        print(f"{name:10s} {result[1]}" + ("   [dry-run]" if dry else ""))


if __name__ == "__main__":
    main()
