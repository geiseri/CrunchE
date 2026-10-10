// Native review-WAV generator: runs the demo song (Tracker::LoadDemoSong)
// and per-invoice isolation/shootout segments through the real synthesis
// code and full-range staging, and writes tests/native/demo_output.wav for
// critical listening. Split out of test_audio.cpp so the gate harness stays
// a quick, hardware-free regression check. Build & run: build_review_wav.sh
//
// WAV layout: P0 mix | P1 drums | P2 bass | P3 lead+pad
//             -> lead shootout (synth2, guitar1, jlead1, synth1) -> pad only
#include "Metrics.h"
#include "OutputMixer.h"
#include "Tracker.h"
#include "Voice.h"
#include "WavWriter.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

double g_millisUs = 0.0;
constexpr long kSteps = 8;  // 120 BPM sixteenths

// Render one demo arp/pad voice in isolation with the demo's own settings.
std::vector<int16_t> renderVoice(bool lead, int instrument) {
  std::vector<int16_t> samples;
  Voice voice;
  voice.SetVolume(1);
  voice.SetEnvelopeLength(lead ? 90000 : 240000);
  static const int kArpNotes[4] = {0, 4, 9, 4};
  static const int kPadNotes[4] = {9, 5, 7, 0};
  for (int step = 0; step < 32; ++step) {
    for (int frame = 0; frame < 2756; ++frame) {
      samples.push_back(StageMasterSample(voice.UpdateVoice(), 1));
    }
    if (lead) {
      voice.SetNote(kArpNotes[step % 4], false, (step / 16) % 2 ? 2 : 1,
                    instrument);
    } else if (step % 8 == 0) {
      voice.SetNote(kPadNotes[(step / 8) % 4], false, 0, instrument);
    }
  }
  return samples;
}

}  // namespace

namespace tracker_clock {
uint32_t Now() { return static_cast<uint32_t>(g_millisUs / 1000.0); }
}

int main() {
  std::printf("== Demo pattern levels (hardware view, div=%d) ==\n", MasterDiv());
  Tracker tr;
  tr.LoadDemoSong();
  std::vector<int16_t> per[4], full[4];
  for (int step = 0; step < 4 * 32; ++step) {
    for (long frame = 0; frame < metrics::kRate / kSteps; ++frame) {
      g_millisUs += 1000000.0 / metrics::kRate;
      const int raw = tr.UpdateTracker();
      per[tr.currentPattern].push_back(StageMasterSample(raw));
      full[tr.currentPattern].push_back(StageMasterSample(raw, 1));
    }
  }
  int failures = 0;
  const char* names[4] = {"P0 full mix", "P1 drums", "P2 bass", "P3 lead+pad"};
  for (int patternIndex = 0; patternIndex < 4; ++patternIndex) {
    if (!metrics::gate(names[patternIndex],
                       metrics::measure(per[patternIndex], 25, 900), 25, 900,
                       300)) {
      ++failures;
    }
  }

  std::vector<int16_t> wav;
  const std::vector<int16_t> gap(4410, 0);
  for (int patternIndex = 0; patternIndex < 4; ++patternIndex) {
    wav.insert(wav.end(), full[patternIndex].begin(),
               full[patternIndex].end());
  }
  const std::pair<int, const char*> kShootout[] = {
      {8, "synth2 (was the buzzing lead)"},
      {13, "guitar1 (current demo lead)"},
      {10, "jlead1 (24ms file, unloopable)"},
      {19, "synth1"},
  };
  std::printf("\n== Lead shootout (demo arp context, informational) ==\n");
  for (const auto& cand : kShootout) {
    auto seg = renderVoice(true, cand.first);
    metrics::line(cand.second, metrics::measure(seg, 150, 1000));
    wav.insert(wav.end(), gap.begin(), gap.end());
    wav.insert(wav.end(), seg.begin(), seg.end());
  }
  auto padOnly = renderVoice(false, 4);
  metrics::line("pad pad1 oct0", metrics::measure(padOnly, 60, 900));
  wav.insert(wav.end(), gap.begin(), gap.end());
  wav.insert(wav.end(), padOnly.begin(), padOnly.end());

  if (writeWav("tests/native/demo_output.wav", wav)) {
    std::printf("WAV written (full range, div=1): tests/native/demo_output.wav  (%.1f s)\n"
                "  layout: mix|drums|bass|lead+pad -> lead:synth2 -> lead:guitar1 ->"
                " lead:jlead1 -> lead:synth1 -> pad:pad1\n",
                wav.size() / metrics::kRate);
  }
  std::printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
  return failures ? 1 : 0;
}
