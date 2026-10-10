// Native regression gates for the CrunchE audio chain — no Arduino, no
// hardware, and deliberately fast: per-instrument level/pitch/click checks
// through the REAL Voice synthesis + shared master staging (OutputMixer.h).
// The demo-song and review-WAV generation live in make_review_wav.cpp
// (build_review_wav.sh) so this file stays a quick sanity gate.
// Build & run: tests/native/build_and_run.sh
#include "Metrics.h"
#include "OutputMixer.h"
#include "Voice.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("  FAIL: %s\n", what);
    ++failures;
  }
}

void runCase(const char* label, int val, int oct, int inst, double loHz,
             double hiHz, bool gating) {
  Voice voice;
  voice.SetVolume(2);
  voice.SetNote(val, false, oct, inst);
  std::vector<int16_t> buf;
  buf.reserve(22050);
  for (int frame = 0; frame < 22050; ++frame) {
    buf.push_back(StageMasterSample(voice.UpdateVoice()));
  }
  auto stats = metrics::measure(buf, loHz, hiHz);
  if (gating) {
    if (!metrics::gate(label, stats, loHz, hiHz, 300)) ++failures;
  } else {
    metrics::line(label, stats);  // timbral sources: informational only
  }
}

void TestSetNoteFamilyPlayheadReset() {
  std::printf("== SetNote InstrumentFamily playhead reset ==\n");
  Voice voice;

  // Pitched → drum: family change must restart the one-shot playhead.
  voice.SetNote(0, false, 1, 4);  // pad1 (pitched)
  for (int frame = 0; frame < 500; ++frame) {
    (void)voice.UpdateVoice();
  }
  check(voice.SampleIndexForTest() > 1.0f, "pitched playhead advanced");
  voice.SetNote(0, false, 0, 0);  // kick (drum)
  check(voice.SampleIndexForTest() == 0.0f, "pitched→drum resets playhead");

  // Drum → pitched: family change resets even though pitched can loop.
  for (int frame = 0; frame < 200; ++frame) {
    (void)voice.UpdateVoice();
  }
  check(voice.SampleIndexForTest() > 1.0f, "drum playhead advanced");
  voice.SetNote(0, false, 1, 4);  // pad1 again
  check(voice.SampleIndexForTest() == 0.0f, "drum→pitched resets playhead");

  // Pitched → pitched: continuous phase (same family, no reset).
  for (int frame = 0; frame < 100; ++frame) {
    (void)voice.UpdateVoice();
  }
  const float before = voice.SampleIndexForTest();
  voice.SetNote(4, false, 1, 9);  // jbass1, still pitched
  check(voice.SampleIndexForTest() == before, "pitched→pitched keeps playhead");
}

}  // namespace

int main() {
  std::printf("== Voice level: instruments through the real chain ==\n");
  // label,                     note, oct, inst,  loHz,  hiHz, gate
  runCase("drums kick", 0, 0, 0, 25, 400, true);
  runCase("bass jbass1", 9, 2, 9, 150, 800, true);
  runCase("lead synth2", 9, 1, 8, 150, 1400, false);
  runCase("pad pad1", 9, 1, 4, 150, 900, false);
  runCase("lead guitar1", 9, 1, 13, 150, 1400, false);
  runCase("bass1 sine o4", 9, 4, 2, 80, 1400, false);

  TestSetNoteFamilyPlayheadReset();

  std::printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
  return failures ? 1 : 0;
}
