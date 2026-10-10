#!/bin/sh
# Full sample pipeline (derived artifacts from frozen Samples_src).
#
# Reference PCM: Samples_src/*.wav — created once by a manual upstream import
# (tools/import_pristine.py against true upstream headers) and verified then.
# This script does NOT re-import or run the pristine `verify` gate.
#
#   loops        Samples_src -> shift/saturate/seam -> Samples_src/loops/*.wav
#   gen_headers  loops/else-pristine -> Samples/*.h + SampleGains.h
#   verifygen    generated headers byte-exact vs their PCM   [wav -> header]
#
# Re-run after adding/replacing any Samples_src WAV, then the native trio.
set -e
cd "$(dirname "$0")/.."
# DSP steps need numpy+scipy (loops.py, gen_headers.py); PlatformIO's venv
# has them - see requirements.txt to (re)install. Fall back to python3 only
# if the venv is missing.
PYTHON="$HOME/.platformio/penv/bin/python"
"$PYTHON" -c "import numpy, scipy" 2>/dev/null || PYTHON=python3
"$PYTHON" tools/loops.py
"$PYTHON" tools/gen_headers.py
# Compile with the WORKSPACE include order: kSamples holds the freshly
# GENERATED numbers, exactly as the firmware will compile them.
c++ -std=gnu++20 -O2 -DMAKE_SAMPLES_GATE=GENERATED \
  -I. -Itests/native tools/make_samples.cpp \
  -o /tmp/verify_gen
/tmp/verify_gen verifygen
