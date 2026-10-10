// Shared audio metrics for the native test programs (header-only).
// measure() reports level (peak/rms), broadband slope energy (fizz),
// slope-normalized click rate (loop-seam/pop detector), and a band-limited
// dominant pitch; report helpers print a consistent line.
#pragma once

#include "AudioConfig.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace metrics {

inline constexpr double kRate = static_cast<double>(kAudioSampleRate);

struct Result {
  double peak = 0;
  double rms = 0;
  double fizzRms = 0;      // first-difference energy: hiss/stair-step proxy
  double clicksPerSec = 0; // slope-normalized |Δ| outliers: seam clicks
  double dominantHz = 0;
};

inline double dominantHz(const std::vector<int16_t>& samples, double loHz,
                         double hiHz) {
  const int length = static_cast<int>(samples.size());
  int minLag = std::max(2, static_cast<int>(kRate / hiHz));
  int maxLag = std::min(length - 2, static_cast<int>(kRate / loHz));
  long energy0 = 0;
  for (int index = 0; index < length; ++index) {
    energy0 += static_cast<long>(samples[index]) * samples[index];
  }
  if (energy0 == 0) return 0.0;
  std::vector<double> norm(maxLag + 1, 0.0);
  for (int lag = 0; lag <= maxLag; ++lag) {
    long acc = 0;
    for (int index = 0; index + lag < length; ++index) {
      acc += static_cast<long>(samples[index]) * samples[index + lag];
    }
    norm[lag] = static_cast<double>(acc) / energy0;
  }
  int bestLag = 0;
  double bestVal = 0.0;
  for (int lag = minLag; lag <= maxLag; ++lag) {
    const double sampleValue = norm[lag];
    if (sampleValue > 0.5 && sampleValue >= norm[lag - 1] &&
        sampleValue > norm[lag + 1] && sampleValue > bestVal) {
      bestVal = sampleValue;
      bestLag = lag;
    }
  }
  if (bestLag == 0) {
    for (int lag = minLag; lag <= maxLag; ++lag) {
      if (norm[lag] > bestVal) {
        bestVal = norm[lag];
        bestLag = lag;
      }
    }
  }
  return bestLag ? kRate / bestLag : 0.0;
}

inline Result measure(const std::vector<int16_t>& buf, double loHz,
                      double hiHz) {
  Result result;
  double sumSq = 0, diffSq = 0, absDiffSum = 0;
  for (size_t index = 0; index < buf.size(); ++index) {
    const double sampleValue = buf[index];
    result.peak = std::max(result.peak, std::fabs(sampleValue));
    sumSq += sampleValue * sampleValue;
    if (index > 0) {
      const double delta = sampleValue - buf[index - 1];
      diffSq += delta * delta;
      absDiffSum += std::fabs(delta);
    }
  }
  result.rms = std::sqrt(sumSq / buf.size());
  result.fizzRms = std::sqrt(diffSq / buf.size());
  const double clickThreshold = (absDiffSum / buf.size()) * 8.0;
  long clicks = 0;
  for (size_t index = 1; index < buf.size(); ++index) {
    if (std::fabs(static_cast<double>(buf[index]) - buf[index - 1]) >
        clickThreshold) {
      ++clicks;
    }
  }
  result.clicksPerSec = clicks / (buf.size() / kRate);
  const std::vector<int16_t> win(buf.begin() + 1000, buf.begin() + 3205);
  result.dominantHz = dominantHz(win, loHz, hiHz);
  return result;
}

inline void line(const char* label, const Result& result) {
  std::printf(
      "%-30s peak=%6.0f rms=%6.0f fizz=%6.0f clicks/s=%6.1f dom=%6.1fHz\n",
      label, result.peak, result.rms, result.fizzRms, result.clicksPerSec,
      result.dominantHz);
}

// Prints the metric line plus a PASS/FAIL verdict on level + pitch band.
inline bool gate(const char* label, const Result& result, double loHz,
                 double hiHz, double minRms) {
  const bool ok = result.rms >= minRms && result.dominantHz >= loHz &&
                  result.dominantHz <= hiHz;
  char full[64];
  std::snprintf(full, sizeof(full), "%s %s", label, ok ? "PASS" : "FAIL");
  full[sizeof(full) - 1] = '\0';
  line(label, result);
  return ok;
}

}  // namespace metrics
