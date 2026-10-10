#pragma once

#include <cstddef>
#include <cstdint>

// Ear-calibration frequency grids. Keep in sync with README.md
// "Ear grid (keypad map → SPEAKER_BAND_HZ)". Index order is silkscreen
// top → bottom, left → right:
//   F1 F2 F3 F4 | G# A A# B | E F F# G | C C# D D#
// Values are expected to stay stable; change only with a clear reason.

constexpr size_t kEarGridCells = 16;

// Usable low edge (Hz).
constexpr float kEarLoHz[kEarGridCells] = {
    530.0f, 610.0f, 700.0f, 800.0f,  // F1 F2 F3 F4
    300.0f, 350.0f, 400.0f, 460.0f,  // G# A  A# B
    175.0f, 200.0f, 230.0f, 260.0f,  // E  F  F# G
    100.0f, 115.0f, 130.0f, 150.0f,  // C  C# D  D#
};

// Before breakup / nasty treble (Hz).
constexpr float kEarHiHz[kEarGridCells] = {
    9000.0f,  9600.0f, 10300.0f, 11000.0f,  // F1 F2 F3 F4
    6900.0f,  7300.0f,  7900.0f,  8400.0f,  // G# A  A# B
    5200.0f,  5600.0f,  6000.0f,  6400.0f,  // E  F  F# G
    4000.0f,  4300.0f,  4600.0f,  4900.0f,  // C  C# D  D#
};
