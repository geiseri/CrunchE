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

inline double dominantHz(const std::vector<int16_t>& x, double loHz, double hiHz) {
  const int n = static_cast<int>(x.size());
  int minLag = std::max(2, static_cast<int>(kRate / hiHz));
  int maxLag = std::min(n - 2, static_cast<int>(kRate / loHz));
  long e0 = 0;
  for (int i = 0; i < n; ++i) e0 += static_cast<long>(x[i]) * x[i];
  if (e0 == 0) return 0.0;
  std::vector<double> norm(maxLag + 1, 0.0);
  for (int lag = 0; lag <= maxLag; ++lag) {
    long acc = 0;
    for (int i = 0; i + lag < n; ++i) {
      acc += static_cast<long>(x[i]) * x[i + lag];
    }
    norm[lag] = static_cast<double>(acc) / e0;
  }
  int bestLag = 0;
  double bestVal = 0.0;
  for (int lag = minLag; lag <= maxLag; ++lag) {
    const double v = norm[lag];
    if (v > 0.5 && v >= norm[lag - 1] && v > norm[lag + 1] && v > bestVal) {
      bestVal = v;
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

inline Result measure(const std::vector<int16_t>& buf, double loHz, double hiHz) {
  Result m;
  double sumSq = 0, diffSq = 0, absDiffSum = 0;
  for (size_t i = 0; i < buf.size(); ++i) {
    const double v = buf[i];
    m.peak = std::max(m.peak, std::fabs(v));
    sumSq += v * v;
    if (i > 0) {
      const double d = v - buf[i - 1];
      diffSq += d * d;
      absDiffSum += std::fabs(d);
    }
  }
  m.rms = std::sqrt(sumSq / buf.size());
  m.fizzRms = std::sqrt(diffSq / buf.size());
  const double clickThreshold = (absDiffSum / buf.size()) * 8.0;
  long clicks = 0;
  for (size_t i = 1; i < buf.size(); ++i) {
    if (std::fabs(static_cast<double>(buf[i]) - buf[i - 1]) > clickThreshold) {
      ++clicks;
    }
  }
  m.clicksPerSec = clicks / (buf.size() / kRate);
  const std::vector<int16_t> win(buf.begin() + 1000, buf.begin() + 3205);
  m.dominantHz = dominantHz(win, loHz, hiHz);
  return m;
}

inline void line(const char* label, const Result& m) {
  std::printf("%-30s peak=%6.0f rms=%6.0f fizz=%6.0f clicks/s=%6.1f dom=%6.1fHz\n",
              label, m.peak, m.rms, m.fizzRms, m.clicksPerSec, m.dominantHz);
}

// Prints the metric line plus a PASS/FAIL verdict on level + pitch band.
inline bool gate(const char* label, const Result& m, double loHz, double hiHz,
                 double minRms) {
  const bool ok = m.rms >= minRms && m.dominantHz >= loHz && m.dominantHz <= hiHz;
  char full[64];
  std::snprintf(full, sizeof(full), "%s %s", label, ok ? "PASS" : "FAIL");
  full[sizeof(full) - 1] = '\0';
  line(label, m);
  return ok;
}

}  // namespace metrics
