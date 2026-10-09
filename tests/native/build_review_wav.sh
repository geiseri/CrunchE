#!/bin/sh
# Generate tests/native/demo_output.wav (demo cycle + isolation/shootout
# segments) and gate the demo pattern levels. No hardware required.
# Listening: afplay tests/native/demo_output.wav
set -e
cd "$(dirname "$0")/../.."
c++ -std=gnu++20 -O2 -I. \
  tests/native/make_review_wav.cpp Tracker.cpp Voice.cpp \
  -o /tmp/crunche_review_wav
/tmp/crunche_review_wav
