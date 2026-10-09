#!/bin/sh
# Native gate: InputManager transitions + full command matrix.
# Pure logic (no Arduino, no Tracker/Voice objects).
set -e
cd "$(dirname "$0")/../.."
c++ -std=gnu++20 -Wall -I. \
  tests/native/test_input.cpp InputManager.cpp \
  -o /tmp/crunche_test_input
exec /tmp/crunche_test_input
