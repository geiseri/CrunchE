#pragma once
#include "AudioConfig.h"

#include <cstdint>
#include <cstdio>
#include <vector>

// 16-bit mono PCM WAV writer shared by the native test programs.
inline bool writeWav(const char* path, const std::vector<int16_t>& pcm,
                     uint32_t sampleRate = kAudioSampleRate) {
  FILE* file = std::fopen(path, "wb");
  if (!file) return false;
  const uint32_t dataBytes = static_cast<uint32_t>(pcm.size() * sizeof(int16_t));
  auto writeU32 = [&](uint32_t value) {
    for (int byteIndex = 0; byteIndex < 4; ++byteIndex) {
      std::fputc((value >> (8 * byteIndex)) & 0xff, file);
    }
  };
  auto writeU16 = [&](uint16_t value) {
    std::fputc(value & 0xff, file);
    std::fputc((value >> 8) & 0xff, file);
  };
  std::fwrite("RIFF", 1, 4, file);
  writeU32(36 + dataBytes);
  std::fwrite("WAVE", 1, 4, file);
  std::fwrite("fmt ", 1, 4, file);
  writeU32(16);
  writeU16(1);
  writeU16(1);
  writeU32(sampleRate);
  writeU32(sampleRate * 2);
  writeU16(2);
  writeU16(16);
  std::fwrite("data", 1, 4, file);
  writeU32(dataBytes);
  std::fwrite(pcm.data(), sizeof(int16_t), pcm.size(), file);
  std::fclose(file);
  return true;
}
