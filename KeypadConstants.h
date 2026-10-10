#pragma once

// Keypad-related constants used by Tracker and the instruction-sheet tools.
// Source of truth: this header (and KeypadMaps.cpp for key → Command tables).
// Electrical note order is NoteKey in InputManager.h (tools parse that enum).

#include <cstdint>

// Bank 1: F1+note arg folds to voiceNum = offset + arg (see Tracker Instrument).
constexpr int kInstrumentBank1Offset = 12;

// Tempo command args 0..3 → BPM (F4 + G#..B).
constexpr int kTempoBpmTable[4] = {120, 132, 145, 180};

// Physical 4×4 membrane layout, top → bottom (docs/InstA.png).
// Parsed by tools/keypad_contract.py — keep labels matching NoteKey / F1..F4.
// KEYPAD-SILK: function row | F1, F2, F3, F4
// KEYPAD-SILK: octave row | G#, A, A#, B
// KEYPAD-SILK: octave row | E, F, F#, G
// KEYPAD-SILK: base notes | C, C#, D, D#
