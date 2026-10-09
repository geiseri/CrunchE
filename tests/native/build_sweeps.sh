#!/bin/sh
# Build and run the sample register sweeps (no hardware required).
# Emits one WAV per waveform into tests/native/sweeps/ for critical listening.
set -e
cd "$(dirname "$0")/../.."
c++ -std=gnu++20 -O2 -I. \
  tests/native/make_sweeps.cpp Voice.cpp \
  -o /tmp/crunche_sweeps
/tmp/crunche_sweeps
