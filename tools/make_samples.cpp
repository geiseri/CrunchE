// C++ sample pipeline VERIFICATION gates. The DSP itself lives in Python
// on proven libraries: loops.py (numpy) applies fixups (+12 shift, tier-2
// saturation, seam loop prep) to Samples_src/loops/*.wav; gen_headers.py
// writes Samples/*.h. Samples_src/*.wav are the frozen reference PCM.
//
//   make_samples verify     MANUAL only: after an upstream header -> wav
//                           import, compare Samples_src to those headers.
//                           Requires -DMAKE_SAMPLES_GATE=PRISTINE and an
//                           -I that resolves Samples/*.h to the upstream
//                           dump (not generated workspace headers).
//                           Not run by tools/gen_all.sh.
//   make_samples verifygen  GENERATED Samples/*.h must byte-equal the PCM
//                           they were written from (loops wav, else
//                           Samples_src). Requires -DMAKE_SAMPLES_GATE=GENERATED
//                           (tools/gen_all.sh builds this).
//
// Fixups always derive from frozen Samples_src, so re-running gen_all.sh
// cannot compound across passes.
#include "AudioConfig.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "Samples/bass1.h"
#include "Samples/bass2.h"
#include "Samples/bongo1.h"
#include "Samples/clap1.h"
#include "Samples/crash1.h"
#include "Samples/guitar1.h"
#include "Samples/hihat1.h"
#include "Samples/hihat2.h"
#include "Samples/jbass1.h"
#include "Samples/jbass2.h"
#include "Samples/jlead1.h"
#include "Samples/jlead2.h"
#include "Samples/jlead3.h"
#include "Samples/jlead4.h"
#include "Samples/jpad1.h"
#include "Samples/kick1.h"
#include "Samples/kick2.h"
#include "Samples/kick3.h"
#include "Samples/pad1.h"
#include "Samples/pad2.h"
#include "Samples/pad3.h"
#include "Samples/pureSin.h"
#include "Samples/pureTriSoft.h"
#include "Samples/ride1.h"
#include "Samples/sfx1.h"
#include "Samples/sfx10.h"
#include "Samples/sfx11.h"
#include "Samples/sfx12.h"
#include "Samples/sfx2.h"
#include "Samples/sfx3.h"
#include "Samples/sfx4.h"
#include "Samples/sfx5.h"
#include "Samples/sfx6.h"
#include "Samples/sfx7.h"
#include "Samples/sfx8.h"
#include "Samples/sfx9.h"
#include "Samples/snare1.h"
#include "Samples/snare2.h"
#include "Samples/snare3.h"
#include "Samples/snareB1.h"
#include "Samples/snareB2.h"
#include "Samples/snareB3.h"
#include "Samples/synth1.h"
#include "Samples/synth2.h"
#include "Samples/synth3.h"

namespace {

constexpr int kRate = static_cast<int>(kAudioSampleRate);
constexpr uint32_t kMaxDataBytes = 50u * 1024u * 1024u;

#define MAKE_SAMPLES_STRINGIFY(x) #x
#define MAKE_SAMPLES_XSTRINGIFY(x) MAKE_SAMPLES_STRINGIFY(x)
#if defined(MAKE_SAMPLES_GATE)
constexpr const char *kMakeSamplesGate = MAKE_SAMPLES_XSTRINGIFY(MAKE_SAMPLES_GATE);
#else
constexpr const char *kMakeSamplesGate = "";
#endif

struct Sample {
  const char *name;
  const int *data;
  int len;
  bool melodic;  // loop-prepped (sustained playback) vs one-shot
};

const Sample kSamples[] = {
    {"bass1", bass1, bass1Length, true},
    {"jbass2", jbass2, jbass2Length, true},
    {"pad1", pad1, pad1Length, true},
    {"jpad1", jpad1, jpad1Length, true},
    {"pad3", pad3, pad3Length, true},
    {"bongo1", bongo1, bongo1Length, true},
    {"synth2", synth2, synth2Length, true},
    {"jbass1", jbass1, jbass1Length, true},
    {"jlead1", jlead1, jlead1Length, true},
    {"jlead2", jlead2, jlead2Length, true},
    {"bass2", bass2, bass2Length, true},
    {"guitar1", guitar1, guitar1Length, true},
    {"jlead3", jlead3, jlead3Length, true},
    {"jlead4", jlead4, jlead4Length, true},
    {"kick3", kick3, kick3Length, true},
    {"pad2", pad2, pad2Length, true},
    {"snareB3", snareB3, snareB3Length, true},
    {"synth1", synth1, synth1Length, true},
    {"synth3", synth3, synth3Length, true},
    {"kick1", kick1, kick1Length, false},
    {"kick2", kick2, kick2Length, false},
    {"snare1", snare1, snare1Length, false},
    {"snare2", snare2, snare2Length, false},
    {"snare3", snare3, snare3Length, false},
    {"snareB1", snareB1, snareB1Length, false},
    {"snareB2", snareB2, snareB2Length, false},
    {"hihat1", hihat1, hihat1Length, false},
    {"hihat2", hihat2, hihat2Length, false},
    {"clap1", clap1, clap1Length, false},
    {"crash1", crash1, crash1Length, false},
    {"ride1", ride1, ride1Length, false},
    {"sfx1", sfx1, sfx1Length, false},
    {"sfx2", sfx2, sfx2Length, false},
    {"sfx3", sfx3, sfx3Length, false},
    {"sfx4", sfx4, sfx4Length, false},
    {"sfx5", sfx5, sfx5Length, false},
    {"sfx6", sfx6, sfx6Length, false},
    {"sfx7", sfx7, sfx7Length, false},
    {"sfx8", sfx8, sfx8Length, false},
    {"sfx9", sfx9, sfx9Length, false},
    {"sfx10", sfx10, sfx10Length, false},
    {"sfx11", sfx11, sfx11Length, false},
    {"sfx12", sfx12, sfx12Length, false},
    {"pureSin", pureSin, pureSinLength, false},
    {"pureTriSoft", pureTriSoft, pureTriSoftLength, false},
};

// Voices sustained at demo key positions alias badly when read at the
// 6.7-13.4x ratios their low source fundamentals require. Shifting these
// +12 semitones offline (x2 linear upsample) halves every runtime ratio at
// the SAME ear-pitch; paired demo octaves step down one. Percussion-voiced
// entries (jbass*, bongo1, kick3) keep their register and are not shifted.
// (The shift/saturation/seam implementation moved to loops.py on numpy.)

struct WavReadResult {
  std::vector<int> pcm;
  int rate = 0;
  std::string error;
};

WavReadResult readWavRaw(const std::string &path) {
  WavReadResult result;
  FILE *file = std::fopen(path.c_str(), "rb");
  if (!file) {
    result.error = "open failed";
    return result;
  }

  auto fail = [&](const char *message) {
    result.pcm.clear();
    result.rate = 0;
    result.error = message;
    std::fclose(file);
    return result;
  };

  char riff[4];
  if (std::fread(riff, 1, 4, file) != 4 || std::memcmp(riff, "RIFF", 4) != 0) {
    return fail("not RIFF");
  }
  std::fseek(file, 4, SEEK_CUR);  // RIFF chunk size
  char wave[4];
  if (std::fread(wave, 1, 4, file) != 4 || std::memcmp(wave, "WAVE", 4) != 0) {
    return fail("not WAVE");
  }

  int channels = 0;
  int fileRate = 0;
  int bits = 0;
  uint16_t fmtTag = 0;
  bool sawFmt = false;
  std::vector<int16_t> pcm;

  while (true) {
    char id[4];
    uint32_t size = 0;
    if (std::fread(id, 1, 4, file) != 4 || std::fread(&size, 4, 1, file) != 1) {
      break;
    }
    const long dataStart = std::ftell(file);
    if (dataStart < 0) {
      return fail("ftell failed");
    }
    // WAV chunk payloads are padded to an even byte count.
    const long next =
        dataStart + static_cast<long>(size) + static_cast<long>(size & 1u);

    if (std::memcmp(id, "fmt ", 4) == 0) {
      if (size < 16) {
        return fail("fmt chunk too small");
      }
      uint16_t ch = 0;
      uint16_t bitDepth = 0;
      uint32_t sr = 0;
      if (std::fread(&fmtTag, 2, 1, file) != 1 ||
          std::fread(&ch, 2, 1, file) != 1 ||
          std::fread(&sr, 4, 1, file) != 1) {
        return fail("fmt read failed");
      }
      std::fseek(file, 6, SEEK_CUR);  // byteRate + blockAlign
      if (std::fread(&bitDepth, 2, 1, file) != 1) {
        return fail("fmt bits read failed");
      }
      channels = ch;
      fileRate = static_cast<int>(sr);
      bits = bitDepth;
      sawFmt = true;
    } else if (std::memcmp(id, "data", 4) == 0) {
      if (size == 0) {
        return fail("empty data chunk");
      }
      if (size > kMaxDataBytes) {
        return fail("data chunk too large");
      }
      if ((size & 1u) != 0) {
        return fail("odd data size (expected 16-bit frames)");
      }
      pcm.resize(size / 2);
      const size_t got = std::fread(pcm.data(), 1, size, file);
      if (got != size) {
        return fail("short data read");
      }
    }

    if (std::fseek(file, next, SEEK_SET) != 0) {
      return fail("chunk seek failed");
    }
  }
  std::fclose(file);

  if (!sawFmt) {
    result.error = "missing fmt chunk";
    return result;
  }
  if (fmtTag != 1) {
    result.error = "not PCM (fmtTag != 1)";
    return result;
  }
  if (channels != 1) {
    result.error = "not mono";
    return result;
  }
  if (bits != 16) {
    result.error = "not 16-bit";
    return result;
  }
  if (fileRate != kRate) {
    result.error = "wrong sample rate";
    return result;
  }
  if (pcm.empty()) {
    result.error = "missing data chunk";
    return result;
  }

  result.rate = fileRate;
  result.pcm.reserve(pcm.size());
  for (int16_t sampleValue : pcm) {
    result.pcm.push_back(sampleValue);
  }
  return result;
}

// Returns true on match. On failure prints one line and returns false.
bool compareSample(const Sample &sample, const WavReadResult &wav,
                   const char *modeLabel, const char *wavPath) {
  if (!wav.error.empty()) {
    std::printf("%-10s %s FAIL vs %s (%s)\n", sample.name, modeLabel, wavPath,
                wav.error.c_str());
    return false;
  }
  if (wav.rate != kRate || static_cast<int>(wav.pcm.size()) != sample.len) {
    std::printf("%-10s %s FAIL vs %s (len/rate got rate=%d len=%d want %d)\n",
                sample.name, modeLabel, wavPath, wav.rate,
                static_cast<int>(wav.pcm.size()), sample.len);
    return false;
  }
  for (int index = 0; index < sample.len; ++index) {
    if (wav.pcm[index] != sample.data[index]) {
      std::printf("%-10s %s FAIL vs %s (sample@%d)\n", sample.name, modeLabel,
                  wavPath, index);
      return false;
    }
  }
  return true;
}

}  // namespace

int main(int argc, char **argv) {
  const std::string mode = argc > 1 ? argv[1] : "verify";

  if (mode == "verify") {
    if (std::strcmp(kMakeSamplesGate, "PRISTINE") != 0) {
      std::printf(
          "verify requires -DMAKE_SAMPLES_GATE=PRISTINE and -I pointing at "
          "upstream headers (manual after import_pristine; not gen_all.sh)\n");
      return 2;
    }
    int issues = 0;
    for (const Sample &sample : kSamples) {
      const std::string path =
          std::string("Samples_src/") + sample.name + ".wav";
      const WavReadResult wav = readWavRaw(path);
      if (!compareSample(sample, wav, "VERIFY", path.c_str())) {
        ++issues;
      }
    }
    if (issues) {
      std::printf("verify: %d FAILURES\n", issues);
      return 1;
    }
    std::printf("verify: all %u wavs byte-exact vs headers\n",
                static_cast<unsigned>(std::size(kSamples)));
    return 0;
  }

  if (mode == "verifygen") {
    if (std::strcmp(kMakeSamplesGate, "GENERATED") != 0) {
      std::printf(
          "verifygen requires -DMAKE_SAMPLES_GATE=GENERATED and workspace "
          "-I order (build via tools/gen_all.sh)\n");
      return 2;
    }
    int issues = 0;
    int checked = 0;
    for (const Sample &sample : kSamples) {
      const std::string loopPath =
          std::string("Samples_src/loops/") + sample.name + ".wav";
      std::string use = loopPath;
      std::error_code ec;
      if (!std::filesystem::exists(loopPath, ec)) {
        use = std::string("Samples_src/") + sample.name + ".wav";
      }
      const WavReadResult wav = readWavRaw(use);
      if (!compareSample(sample, wav, "VERIFYGEN", use.c_str())) {
        ++issues;
      }
      ++checked;
    }
    if (issues) {
      std::printf("verifygen: %d FAILURES of %d checked\n", issues, checked);
      return 1;
    }
    std::printf("verifygen: all %d generated headers byte-exact vs PCM\n",
                checked);
    return 0;
  }

  std::printf("usage: make_samples verify|verifygen\n");
  return 2;
}
