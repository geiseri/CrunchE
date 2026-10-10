#!/bin/sh
# Native gate: multi-note recording flows (InputManager + Tracker + Voice).
# Simulated clock lives in the test; no Arduino, no WAV output.
set -e
cd "$(dirname "$0")/../.."
c++ -std=gnu++20 -Wall -I. \
  tests/native/test_tracker.cpp InputManager.cpp KeypadMaps.cpp \
  Tracker.cpp Voice.cpp \
  -o /tmp/crunche_test_tracker
exec /tmp/crunche_test_tracker
