#pragma once
#include <cstdint>

// Single source of truth for the audio sample rate. Firmware (I2S init),
// make_samples (loops/verify), the harness/sweeps/stacks timing models, and
// review WAV stamps all derive from this value.
// tools/samplelib.py mirrors it (RATE) - change both together, then re-run
// tools/gen_all.sh and the native test gates.
//
// History: the original Arduino sketch ran 22000 ("22kHz" in the README);
// the native tooling standardized on 22050 (the Audacity/FFmpeg-native
// rate). The 0.23% difference is ~4 cents, so no existing content goes
// out of tune - this header just ends the drift.
constexpr uint32_t kAudioSampleRate = 22050;
