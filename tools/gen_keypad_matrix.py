#!/usr/bin/env python3
"""Emit tests/native/keypad_matrix.generated.inc from KeypadMaps.cpp."""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from keypad_contract import write_matrix_inc  # noqa: E402

if __name__ == "__main__":
    path = write_matrix_inc()
    print(f"wrote {path}")
