#!/usr/bin/env python3
"""One-time pristine import: git HEAD sample headers -> Samples_src/*.wav.

Dumps the raw PCM numbers verbatim - no shift, no gain, no filtering. These
WAVs are the frozen canonical copy of the upstream data; every fixup in the
pipeline (make_samples loops, gen_headers.py gains) is derived from them on
each run, so processing can never compound across passes.

Refuses to clobber an existing wav unless --force is given (a --force dump
re-extracts from the pristine headers themselves, never from generated
artifacts, so even a forced re-import is safe and deterministic).

Run from the repo root (gen_all.sh does this for you):
    python3 tools/import_pristine.py [--force]
"""
import pathlib
import sys
import wave

import samplelib

PRISTINE_DIR = pathlib.Path(".pipeline/pristine_headers/Samples")


def write_wav(path, vals):
    clamped = [max(-32768, min(32767, v)) for v in vals]
    if any(c != v for c, v in zip(clamped, vals)):
        print(f"{path.name}: WARNING: {sum(c != v for c, v in zip(clamped, vals))}"
              f" values clamped to int16")
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(samplelib.RATE)
        w.writeframes(memoryview(array_of(clamped)).tobytes())


def array_of(vals):
    from array import array
    a = array("h")
    a.fromlist(vals)
    return a


def main():
    force = "--force" in sys.argv
    if not PRISTINE_DIR.is_dir():
        sys.exit(f"missing {PRISTINE_DIR} - run via tools/gen_all.sh "
                 f"(it materializes git HEAD headers into .pipeline/)")
    samplelib.SRC_DIR.mkdir(exist_ok=True)
    written = skipped = missing = 0
    for name in samplelib.ALL:
        hdr = PRISTINE_DIR / f"{name}.h"
        if not hdr.exists():
            print(f"{name:12s} NO PRISTINE HEADER")
            missing += 1
            continue
        out = samplelib.SRC_DIR / f"{name}.wav"
        if out.exists() and not force:
            skipped += 1
            continue
        _, _, vals = samplelib.load_header(hdr)
        write_wav(out, vals)
        print(f"{name:12s} imported {len(vals)} samples (raw, verbatim)")
        written += 1
    print(f"import_pristine: {written} written, {skipped} pre-existing kept, "
          f"{missing} missing")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
