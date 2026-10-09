#!/bin/sh
# Quick native regression gates (Voice engine only - no Tracker, no WAV).
set -e
cd "$(dirname "$0")/../.."
c++ -std=gnu++20 -O2 -I. \
  tests/native/test_audio.cpp Voice.cpp \
  -o /tmp/crunche_audio_test
/tmp/crunche_audio_test
