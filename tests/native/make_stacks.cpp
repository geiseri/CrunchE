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
  int a, b;
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
double roughnessPct(const std::vector<int16_t>& x) {
  static const int kWin = 110;  // ~200 Hz envelope lowpass
  std::vector<double> env(x.size());
  double acc = 0;
  for (size_t i = 0; i < x.size(); ++i) {
    acc += std::fabs(static_cast<double>(x[i]));
    if (i >= static_cast<size_t>(kWin)) acc -= std::fabs(static_cast<double>(x[i - kWin]));
    env[i] = acc / kWin;
  }
  double dc = 0;
  for (double e : env) dc += e;
  dc /= env.size();
  if (dc <= 0) return 0.0;
  double mod = 0, total = 0;
  for (double f = 20; f <= 200; f *= 1.15) {
    const double w = 2 * M_PI * f / kRate, coeff = 2 * cos(w);
    double s1 = 0, s2 = 0;
    for (double e : env) {
      const double s = (e - dc) + coeff * s1 - s2;
      s2 = s1;
      s1 = s;
    }
    mod += std::sqrt(s1 * s1 + s2 * s2 - coeff * s1 * s2) / env.size();
    total += 1;
  }
  return 100.0 * (mod / total) / dc;
}

void driveVoices(Voice& arp, Voice& pad, int t, int ia, int ib) {
  static const int notesA[4] = {0, 4, 9, 4};
  static const int notesB[4] = {9, 5, 7, 0};
  if (t % kStepLen == 0) {
    const int step = t / kStepLen;
    arp.SetNote(notesA[step % 4], false, (step / 16) % 2 ? 3 : 2, ia);
    if (step % 8 == 0) {
      pad.SetNote(notesB[(step / 8) % 4], false, kPadOct, ib);
    }
  }
}

PairResult testPair(int ia, int ib, std::vector<int16_t>* capture) {
  Voice arp, pad;
  arp.SetVolume(1);
  arp.SetEnvelopeLength(90000);
  pad.SetVolume(1);
  pad.SetEnvelopeLength(240000);

  PairResult r{ia, ib, 0.0, 0, 0, 0.0, 0.0};
  long over = 0, n = 0;
  int run = 0;
  double sumSq = 0;
  std::vector<int16_t> buf;
  buf.reserve(kSeconds * kRate);
  for (int t = 0; t < kSeconds * kRate; ++t) {
    driveVoices(arp, pad, t, ia, ib);
    const int raw = arp.UpdateVoice() + pad.UpdateVoice();
    const int mag = raw < 0 ? -raw : raw;
    if (mag > kKnee) {
      ++over;
      r.longestRun = std::max(r.longestRun, ++run);
      int bent = kKnee + (mag - kKnee) / 2;
      if (bent >= 30000) ++r.capHits;
    } else {
      run = 0;
    }
    const int16_t v = StageMasterSample(raw, 1);
    sumSq += static_cast<double>(v) * v;
    ++n;
    buf.push_back(v);
  }
  if (capture) *capture = std::move(buf);
  r.kneePct = 100.0 * over / n;
  r.rms = std::sqrt(sumSq / n);
  r.roughPct = roughnessPct(buf);
  return r;
}

}  // namespace

int main() {
  std::vector<PairResult> results;
  for (int a = 2; a <= 20; ++a) {
    for (int b = a + 1; b <= 20; ++b) {
      results.push_back(testPair(a, b, nullptr));
    }
  }
  std::sort(results.begin(), results.end(),
            [](const PairResult& x, const PairResult& y) {
              return x.roughPct != y.roughPct   ? x.roughPct > y.roughPct
                     : x.longestRun != y.longestRun ? x.longestRun > y.longestRun
                                                    : x.kneePct > y.kneePct;
            });

  int fails = 0;
  std::printf("%d pairs; gate: knee%% <= %.2f, sustained run <= %d samples\n",
              static_cast<int>(results.size()), kKneePctLimit,
              kSustainedRunLimit);
  std::printf("worst 12:\n  %-22s %8s %10s %7s %7s %8s\n", "pair", "knee%",
              "maxRunMs", "caps", "rms", "rough%");
  for (size_t i = 0; i < results.size(); ++i) {
    const auto& r = results[i];
    const bool bad =
        r.kneePct > kKneePctLimit || r.longestRun > kSustainedRunLimit;
    if (bad) ++fails;
    if (i >= 12) continue;  // display only the top 12 offenders
    char label[48];
    std::snprintf(label, sizeof(label), "%s + %s", kNames[r.a], kNames[r.b]);
    const double runMs = 1000.0 * r.longestRun / kRate;
    std::printf("  %-22s %8.2f %10.1f %7d %7.0f %8.2f %s\n", label, r.kneePct,
                runMs, r.capHits, r.rms, r.roughPct, bad ? "FAIL" : "");
  }

  if (!results.empty()) {
    const auto& worst = results.front();
    std::vector<int16_t> wav;
    testPair(worst.a, worst.b, &wav);
    const std::string path = std::string("tests/native/sweeps/stack_worst_") +
                             kNames[worst.a] + "_" + kNames[worst.b] + ".wav";
    if (writeWav(path.c_str(), wav)) {
      std::printf("audition worst pair: %s\n", path.c_str());
    }
  }
  std::printf("%s\n", fails ? "RESULT: FAIL" : "RESULT: PASS");
  return fails ? 1 : 0;
}
