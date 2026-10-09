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

void runCase(const char* label, int val, int oct, int inst, double loHz,
             double hiHz, bool gating) {
  Voice v;
  v.SetVolume(2);
  v.SetNote(val, false, oct, inst);
  std::vector<int16_t> buf;
  buf.reserve(22050);
  for (int i = 0; i < 22050; ++i) {
    buf.push_back(StageMasterSample(v.UpdateVoice()));
  }
  auto m = metrics::measure(buf, loHz, hiHz);
  if (gating) {
    if (!metrics::gate(label, m, loHz, hiHz, 300)) ++failures;
  } else {
    metrics::line(label, m);  // timbral sources: informational only
  }
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

  std::printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
  return failures ? 1 : 0;
}
