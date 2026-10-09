#!/bin/sh
# Full sample pipeline.
#
# Pristine source of truth: the UPSTREAM headers at git HEAD, imported ONCE
# into Samples_src/*.wav by import_pristine.py (raw PCM, verbatim numbers).
# The import is skipped while those wavs exist, and BOTH directions are then
# proven natively by the C++ compiler's own view of the data:
#   verify    pristine wavs byte-exact vs HEAD headers   [header -> wav gate]
#   verifygen generated headers byte-exact vs shipped PCM [wav -> header gate]
# Fixups (+12 shift, tier-2 saturation, seam loops, tier-1 gains) are always
# derived from the frozen originals and can never compound across runs.
#
#   import    git HEAD Samples/*.h -> Samples_src/*.wav   [one-time, python]
#   verify    Samples_src wavs byte-exact vs HEAD         [pristine gate]
#   loops     pristine wavs -> shift/saturate/seam -> Samples_src/loops/*.wav
#   gen_headers  loops/else-pristine -> Samples/*.h + SampleGains.h
#   verifygen generated headers byte-exact vs their PCM   [round-trip gate]
#
# Re-run after adding/replacing any Samples_src WAV, then the native trio.
set -e
cd "$(dirname "$0")/.."
# DSP steps need numpy+scipy (loops.py, gen_headers.py); PlatformIO's venv
# has them - see requirements.txt to (re)install. Fall back to python3 only
# if the venv is missing.
PYTHON="$HOME/.platformio/penv/bin/python"
"$PYTHON" -c "import numpy, scipy" 2>/dev/null || PYTHON=python3
PRISTINE=.pipeline/pristine_headers
rm -rf "$PRISTINE"
mkdir -p .pipeline "$PRISTINE"
git archive HEAD Samples | tar -x -C "$PRISTINE"
# -I$PRISTINE goes FIRST so #include "Samples/x.h" resolves to the upstream
# numbers, not the generated build artifacts in ./Samples/
c++ -std=gnu++20 -O2 -I"$PRISTINE" -Itests/native -I. tools/make_samples.cpp \
  -o /tmp/make_samples
"$PYTHON" tools/import_pristine.py
/tmp/make_samples verify
"$PYTHON" tools/loops.py
"$PYTHON" tools/gen_headers.py
# Second compile with the WORKSPACE include order: kSamples now holds the
# freshly GENERATED numbers, exactly as the firmware will compile them.
c++ -std=gnu++20 -O2 -I. -Itests/native tools/make_samples.cpp \
  -o /tmp/verify_gen
/tmp/verify_gen verifygen
