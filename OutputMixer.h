#pragma once
#include <cstdint>

// Master output staging shared by the firmware loop and the native test
// harness, so off-device measurements reflect the exact signal path the
// speaker receives.
//
// Headroom: each voice saturates at +/-16000 (Voice.cpp), a 4-voice sum is
// bounded by +/-64000; the 8000 knee halves the excess and 30000 caps the
// worst case inside int16.
//
// Single safety limiter — the ONLY nonlinear stage in the audio path (voices
// emit raw, contract-bounded samples; see Voice.cpp UpdateVoice).
//
// Headroom contract: source-gain table keeps a volume-1 voice near ±9000
// peak (volume 2 doubles it deliberately; overdrive self-caps at 8000), so
// typical 2-voice program tops out ~18000 — inside the soft region only at
// aligned peaks — while the 4-voice pathological sum (72000) maps through
// the knee (44000) to the 30000 hard cap. The division is the board loudness
// trim (see kMasterDiv); rounding it symmetrically avoids the truncation DC
// bias that /4 on mixed-sign ints would otherwise add.
constexpr int kMasterDiv = 4;  // factory default (= trim step index 3)

// Runtime master loudness trim: F4+F# steps down, F4+G steps up
// (InputManager 'M' -> Tracker::SetCommand). C++17 inline variables give
// one shared state across TUs. Even the loudest step (div 1) stays inside
// int16 because the limiter caps at 30000.
inline constexpr int kMasterTrimTable[] = {12, 8, 6, 4, 3, 2, 1};
inline int g_masterTrimStep = 3;

inline int MasterDiv() { return kMasterTrimTable[g_masterTrimStep]; }

inline void StepMasterTrim(bool up) {
  if (up) {
    if (g_masterTrimStep < 6) ++g_masterTrimStep;
  } else if (g_masterTrimStep > 0) {
    --g_masterTrimStep;
  }
}

// div <= 0 means "use the live trim step" (firmware + device-view harness
// path); explicit div is reserved for full-range renders.
inline int16_t StageMasterSample(int sample, int div = 0) {
  const int mag = sample < 0 ? -sample : sample;
  int out = mag;
  if (out > 16000) {
    out = 16000 + (out - 16000) / 2;
  }
  if (out > 30000) {
    out = 30000;
  }
  if (sample < 0) {
    out = -out;
  }
  const int d = div > 0 ? div : MasterDiv();
  // Round-half-up on magnitude, preserving sign (truncating /4 biases
  // small negative samples toward zero differently than positives).
  const int q = out >= 0 ? (out + d / 2) / d : -((-out + d / 2) / d);
  return static_cast<int16_t>(q);
}
