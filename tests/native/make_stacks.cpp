// Pairwise stack test: the sweeps play voices alone and the harness plays
// the fixed demo, but the intermodulation buzz lived BETWEEN those — two
// sustained voices whose summed peaks cross the master knee, so each voice's
// gain rides the other's waveform (heard as roughness only when they
// overlap). This test renders every melodic instrument pair for 1 s with
// demo-like roles (A = arp retrigging every 16th, B = pad retrigging every
// half bar), then gates on the LONGEST continuous run of summed samples
// beyond the knee (>100 ms = sustained bending = FAIL) plus knee/cap hit
// rates. Re-run after ANY change to OutputMixer.h gains/knees or the source
// gain table. The worst pair is written to a WAV for audition.
#include "AudioConfig.h"
#include "OutputMixer.h"
#include "Voice.h"
#include "WavWriter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr int kRate = static_cast<int>(kAudioSampleRate);
constexpr int kSeconds = 1;
constexpr int kStepLen = kRate / 8;  // 120 BPM sixteenths
constexpr int kKnee = 16000;         // must match StageMasterSample
constexpr int kPadOct = 1;           // mirrors demo: pad a below the arp (set 2 to re-create the old buzzing regime)
constexpr int kSustainedRunLimit = kRate / 10;  // 100 ms = FAIL by itself
// Gate calibrated on the two historical mixers: an 8000 knee bends 5-7% of
// samples in peak-coincident pairs (audible intermodulation ticks, the P3
// buzz); the 16000 knee bends < 0.05%. Anything over 0.5% means sustained
// voices are summing past the knee regularly.
constexpr double kKneePctLimit = 0.5;

const char* kNames[] = {"-", "-", "bass1", "jbass2", "pad1", "jpad1", "pad3",
                        "bongo1", "synth2", "jbass1", "jlead1", "jlead2",
                        "bass2", "guitar1", "jlead3", "jlead4", "kick3",
                        "pad2", "snareB3", "synth1", "synth3"};

struct PairResult {
  int instrumentA, instrumentB;
  double kneePct;
  int longestRun;
  int capHits;
  double rms;
  double roughPct;  // 20-200 Hz energy in the demodulated envelope
};

// Beating detector: two unrelated sustained voices with near-coincident
// partials amplitude-modulate each other; that modulation (20-200 Hz on the
// signal envelope) is what ears hear as buzz/fuzz even when the sample is
// perfectly linear. Reported (not yet gated) alongside knee exposure.
double roughnessPct(const std::vector<int16_t>& samples) {
  static const int kWin = 110;  // ~200 Hz envelope lowpass
  std::vector<double> env(samples.size());
  double acc = 0;
  for (size_t frame = 0; frame < samples.size(); ++frame) {
    acc += std::fabs(static_cast<double>(samples[frame]));
    if (frame >= static_cast<size_t>(kWin)) {
      acc -= std::fabs(static_cast<double>(samples[frame - kWin]));
    }
    env[frame] = acc / kWin;
  }
  double dc = 0;
  for (double envelope : env) dc += envelope;
  dc /= env.size();
  if (dc <= 0) return 0.0;
  double mod = 0, total = 0;
  for (double freq = 20; freq <= 200; freq *= 1.15) {
    const double omega = 2 * M_PI * freq / kRate, coeff = 2 * cos(omega);
    double state1 = 0, state2 = 0;
    for (double envelope : env) {
      const double state = (envelope - dc) + coeff * state1 - state2;
      state2 = state1;
      state1 = state;
    }
    mod += std::sqrt(state1 * state1 + state2 * state2 - coeff * state1 * state2) /
           env.size();
    total += 1;
  }
  return 100.0 * (mod / total) / dc;
}

void driveVoices(Voice& arp, Voice& pad, int time, int instrumentA,
                 int instrumentB) {
  static const int notesA[4] = {0, 4, 9, 4};
  static const int notesB[4] = {9, 5, 7, 0};
  if (time % kStepLen == 0) {
    const int step = time / kStepLen;
    arp.SetNote(notesA[step % 4], false, (step / 16) % 2 ? 3 : 2, instrumentA);
    if (step % 8 == 0) {
      pad.SetNote(notesB[(step / 8) % 4], false, kPadOct, instrumentB);
    }
  }
}

PairResult testPair(int instrumentA, int instrumentB,
                    std::vector<int16_t>* capture) {
  Voice arp, pad;
  arp.SetVolume(1);
  arp.SetEnvelopeLength(90000);
  pad.SetVolume(1);
  pad.SetEnvelopeLength(240000);

  PairResult result{instrumentA, instrumentB, 0.0, 0, 0, 0.0, 0.0};
  long over = 0, count = 0;
  int run = 0;
  double sumSq = 0;
  std::vector<int16_t> buf;
  buf.reserve(kSeconds * kRate);
  for (int time = 0; time < kSeconds * kRate; ++time) {
    driveVoices(arp, pad, time, instrumentA, instrumentB);
    const int raw = arp.UpdateVoice() + pad.UpdateVoice();
    const int mag = raw < 0 ? -raw : raw;
    if (mag > kKnee) {
      ++over;
      result.longestRun = std::max(result.longestRun, ++run);
      int bent = kKnee + (mag - kKnee) / 2;
      if (bent >= 30000) ++result.capHits;
    } else {
      run = 0;
    }
    const int16_t sampleValue = StageMasterSample(raw, 1);
    sumSq += static_cast<double>(sampleValue) * sampleValue;
    ++count;
    buf.push_back(sampleValue);
  }
  if (capture) *capture = std::move(buf);
  result.kneePct = 100.0 * over / count;
  result.rms = std::sqrt(sumSq / count);
  result.roughPct = roughnessPct(buf);
  return result;
}

}  // namespace

int main() {
  std::vector<PairResult> results;
  for (int instrumentA = 2; instrumentA <= 20; ++instrumentA) {
    for (int instrumentB = instrumentA + 1; instrumentB <= 20; ++instrumentB) {
      results.push_back(testPair(instrumentA, instrumentB, nullptr));
    }
  }
  std::sort(results.begin(), results.end(),
            [](const PairResult& left, const PairResult& right) {
              return left.roughPct != right.roughPct
                         ? left.roughPct > right.roughPct
                     : left.longestRun != right.longestRun
                         ? left.longestRun > right.longestRun
                         : left.kneePct > right.kneePct;
            });

  int fails = 0;
  std::printf("%d pairs; gate: knee%% <= %.2f, sustained run <= %d samples\n",
              static_cast<int>(results.size()), kKneePctLimit,
              kSustainedRunLimit);
  std::printf("worst 12:\n  %-22s %8s %10s %7s %7s %8s\n", "pair", "knee%",
              "maxRunMs", "caps", "rms", "rough%");
  for (size_t index = 0; index < results.size(); ++index) {
    const auto& result = results[index];
    const bool bad =
        result.kneePct > kKneePctLimit || result.longestRun > kSustainedRunLimit;
    if (bad) ++fails;
    if (index >= 12) continue;  // display only the top 12 offenders
    char label[48];
    std::snprintf(label, sizeof(label), "%s + %s", kNames[result.instrumentA],
                  kNames[result.instrumentB]);
    const double runMs = 1000.0 * result.longestRun / kRate;
    std::printf("  %-22s %8.2f %10.1f %7d %7.0f %8.2f %s\n", label,
                result.kneePct, runMs, result.capHits, result.rms,
                result.roughPct, bad ? "FAIL" : "");
  }

  if (!results.empty()) {
    const auto& worst = results.front();
    std::vector<int16_t> wav;
    testPair(worst.instrumentA, worst.instrumentB, &wav);
    const std::string path =
        std::string("tests/native/sweeps/stack_worst_") +
        kNames[worst.instrumentA] + "_" + kNames[worst.instrumentB] + ".wav";
    if (writeWav(path.c_str(), wav)) {
      std::printf("audition worst pair: %s\n", path.c_str());
    }
  }
  std::printf("%s\n", fails ? "RESULT: FAIL" : "RESULT: PASS");
  return fails ? 1 : 0;
}
