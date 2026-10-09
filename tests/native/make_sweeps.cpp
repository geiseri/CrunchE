// Sample register sweeps: for every waveform in the library, render the
// full keypad range (12 notes x 4 octaves) into a WAV via the REAL Voice
// synthesis chain and the full-range master staging (div=1). For melodic
// instruments the 12 notes are chromatic pitches; for drums/sfx the note
// selects the sample (each file therefore sweeps all 12 sources x 4 octave
// settings). Waveforms NOT wired into the engine tables (bass2, guitar1,
// jlead3/4, kick3, pad2, pureSin/pureTriSoft, snareB3, synth1/3) are
// rendered "extra_" files using the engine-identical playback algorithm,
// and their measured peak/RMS/suggested-gain are printed as ready-made
// instrumentSources rows should you adopt them.
// Listening to a sweep answers "what IS this sample, and in
// which registers is it audible/clean" independent of hardware.
// Build & run: tests/native/build_sweeps.sh
//
// Sweep layout inside each file:
//   - 0.3 s segments in keypad note order (C C# D D#, E F F# G, G# A A# B),
//     separated by 0.15 s silence;
//   - octaves ascend 0 -> 3, separated by a long 0.5 s gap.
#include "AudioConfig.h"
#include "OutputMixer.h"
#include "Voice.h"
#include "WavWriter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

// Samples not wired into the engine tables (kept as lookup tables),
// included for sweeps only.
#include "Samples/pureSin.h"
#include "Samples/pureTriSoft.h"

namespace {

constexpr double kRate = static_cast<double>(kAudioSampleRate);
constexpr int kSegSamples = static_cast<int>(kRate * 0.3);
constexpr int kGapShort = static_cast<int>(kRate * 0.15);
constexpr int kGapOctave = static_cast<int>(kRate * 0.5);

void renderSweep(const std::string& path, int instrument) {
  std::vector<int16_t> wav;
  for (int octave = 0; octave <= 3; ++octave) {
    if (octave > 0) {
      wav.insert(wav.end(), kGapOctave, 0);
    }
    for (int note = 0; note < 12; ++note) {
      Voice voice;
      voice.SetVolume(1);
      voice.SetEnvelopeNum(2);         // sustain gate: constant level while held
      voice.SetEnvelopeLength(300000); // ~1.3 s, longer than any segment
      voice.SetNote(note, false, octave, instrument);
      for (int i = 0; i < kSegSamples; ++i) {
        wav.push_back(StageMasterSample(voice.UpdateVoice(), 1));
      }
      wav.insert(wav.end(), kGapShort, 0);
    }
  }
  if (writeWav(path.c_str(), wav)) {
    std::printf("%-48s %5.1f s\n", path.c_str(), wav.size() / kRate);
  } else {
    std::printf("FAILED to write %s\n", path.c_str());
  }
}

}  // namespace

// Engine-identical melodic playback for waveforms absent from the
// instrumentSources table (equal-tempered ratio, fmod wrap, linear
// interpolation, RMS-target gain), plus the measured stats that would form
// its table row if adopted.
namespace {

struct ExtraSample {
  const char* name;
  std::span<const int> data;
};

void renderExtra(const std::string& path, const ExtraSample& s) {
  double sumSq = 0;
  int peak = 0;
  for (int v : s.data) {
    peak = std::max(peak, std::abs(v));
    sumSq += static_cast<double>(v) * v;
  }
  const double rms = std::sqrt(sumSq / s.data.size());
  const float gain = static_cast<float>(
      std::min(3400.0 / (rms > 1.0 ? rms : 1.0), peak ? 9000.0 / peak : 1.0));

  std::vector<int16_t> wav;
  const int len = static_cast<int>(s.data.size());
  for (int octave = 0; octave <= 3; ++octave) {
    if (octave > 0) {
      wav.insert(wav.end(), kGapOctave, 0);
    }
    for (int note = 0; note < 12; ++note) {
      const float ratio = std::pow(2.0f, (note + octave * 12) / 12.0f);
      float idx = 0.0f;
      for (int i = 0; i < kSegSamples; ++i) {
        const int i0 = static_cast<int>(idx);
        const int i1 = (i0 + 1 >= len) ? 0 : i0 + 1;
        const double frac = idx - i0;
        const int v = static_cast<int>((s.data[i0] + frac * (s.data[i1] - s.data[i0])) * gain);
        wav.push_back(StageMasterSample(v, 1));
        idx = std::fmod(idx + ratio, static_cast<float>(len));
      }
      wav.insert(wav.end(), kGapShort, 0);
    }
  }
  if (writeWav(path.c_str(), wav)) {
    std::printf("%-48s %5.1f s   peak=%6d rms=%6.0f  gain=%.3f\n",
                path.c_str(), wav.size() / kRate, peak, rms, gain);
  } else {
    std::printf("FAILED to write %s\n", path.c_str());
  }
}

}  // namespace

int main() {
  std::filesystem::create_directories("tests/native/sweeps");
  const char* melodic[] = {"bass1", "jbass2", "pad1", "jpad1", "pad3",
                           "bongo1", "synth2", "jbass1", "jlead1", "jlead2",
                           // instrument bank 1 (F4+D, then F1+note)
                           "bass2", "guitar1", "jlead3", "jlead4", "kick3",
                           "pad2", "snareB3", "synth1", "synth3"};
  for (int i = 0; i < 19; ++i) {
    // Instrument index = voiceNum for the melodic table (2..11).
    renderSweep("tests/native/sweeps/melodic" + std::to_string(i + 2) + "_" +
                    melodic[i] + ".wav",
                i + 2);
  }
  renderSweep("tests/native/sweeps/drums_all12.wav", 0);
  renderSweep("tests/native/sweeps/sfx_all12.wav", 1);

  const ExtraSample extras[] = {
      {"pureSin", {pureSin, static_cast<size_t>(pureSinLength)}},
      {"pureTriSoft", {pureTriSoft, static_cast<size_t>(pureTriSoftLength)}},
  };
  for (const auto& s : extras) {
    renderExtra(std::string("tests/native/sweeps/extra_") + s.name + ".wav", s);
  }
  std::printf("done: %d files in tests/native/sweeps/\n", 19 + 2 + static_cast<int>(std::size(extras)));
  return 0;
}
