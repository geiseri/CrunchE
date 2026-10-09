#pragma once
#include "AudioConfig.h"

#include <cstdint>
#include <cstdio>
#include <vector>

// 16-bit mono PCM WAV writer shared by the native test programs.
inline bool writeWav(const char* path, const std::vector<int16_t>& pcm,
                     uint32_t sampleRate = kAudioSampleRate) {
  FILE* f = std::fopen(path, "wb");
  if (!f) return false;
  const uint32_t dataBytes = static_cast<uint32_t>(pcm.size() * sizeof(int16_t));
  auto w32 = [&](uint32_t v) {
    for (int i = 0; i < 4; ++i) std::fputc((v >> (8 * i)) & 0xff, f);
  };
  auto w16 = [&](uint16_t v) {
    std::fputc(v & 0xff, f);
    std::fputc((v >> 8) & 0xff, f);
  };
  std::fwrite("RIFF", 1, 4, f); w32(36 + dataBytes);
  std::fwrite("WAVE", 1, 4, f);
  std::fwrite("fmt ", 1, 4, f); w32(16); w16(1); w16(1);
  w32(sampleRate); w32(sampleRate * 2); w16(2); w16(16);
  std::fwrite("data", 1, 4, f); w32(dataBytes);
  std::fwrite(pcm.data(), sizeof(int16_t), pcm.size(), f);
  std::fclose(f);
  return true;
}
