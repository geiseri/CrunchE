#!/bin/sh
# Derive printout + native matrix from keypad C++ (source of truth).
# Edit KeypadMaps.cpp / KeypadConstants.h, then run this.
set -e
cd "$(dirname "$0")/.."
PY="${HOME}/.platformio/penv/bin/python"
if [ ! -x "$PY" ]; then
  PY=python3
fi
"$PY" tools/gen_keypad_matrix.py
"$PY" tools/make_instruction_image.py
