#!/bin/sh
# Native gate: InputManager transitions + full command matrix.
# Pure logic (no Arduino, no Tracker/Voice objects).
set -e
cd "$(dirname "$0")/../.."
# Refresh matrix include from KeypadMaps.cpp when the maps are newer.
if [ ! -f tests/native/keypad_matrix.generated.inc ] ||
   [ KeypadMaps.cpp -nt tests/native/keypad_matrix.generated.inc ]; then
  if [ -x "$HOME/.platformio/penv/bin/python" ]; then
    "$HOME/.platformio/penv/bin/python" tools/gen_keypad_matrix.py
  else
    python3 tools/gen_keypad_matrix.py
  fi
fi
c++ -std=gnu++20 -Wall -I. -Itests/native \
  tests/native/test_input.cpp InputManager.cpp KeypadMaps.cpp \
  -o /tmp/crunche_test_input
exec /tmp/crunche_test_input
