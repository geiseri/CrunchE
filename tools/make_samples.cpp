// C++ sample pipeline VERIFICATION gates. The DSP itself lives in Python
// on proven libraries: import_pristine.py (stdlib) dumps git HEAD headers
// verbatim into Samples_src/*.wav, and loops.py (numpy) applies the fixups
// (+12 shift, tier-2 saturation, seam loop prep) to Samples_src/loops/*.wav.
//
//   make_samples verify   re-read every Samples_src wav, compare byte-exact
//                         to the compiled HEAD headers (the pristine gate:
//                         catches any mis-parse by import_pristine.py)
//   make_samples verifygen  [second binary, workspace include order] the
//                         GENERATED Samples/*.h must byte-equal the PCM they
//                         were written from (loops wav, else pristine wav).
//                         Proves the wav -> header direction natively, so
//                         Python never needs trust: it only formats.
//
// The compiler's own view of the header numbers is ground truth in both
// directions; re-running the pipeline is idempotent because fixups derive
// only from the frozen pristine wavs.
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

struct Sample {
  const char* name;
  const int* data;
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

std::vector<int> readWavRaw(const std::string& path, int* rate) {
  std::vector<int> out;
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return out;
  char riff[4];
  std::fread(riff, 1, 4, f);
  if (std::memcmp(riff, "RIFF", 4) != 0) { std::fclose(f); return out; }
  std::fseek(f, 4, SEEK_CUR);  // RIFF chunk size
  char wave[4];
  if (std::fread(wave, 1, 4, f) != 4 || std::memcmp(wave, "WAVE", 4) != 0) {
    std::fclose(f);
    return out;
  }
  // Walk chunks; keep fmt channel/rate + data frames.
  int channels = 0, fileRate = 0, bits = 0;
  std::vector<int16_t> pcm;
  while (std::feof(f) == 0) {
    char id[4];
    uint32_t size = 0;
    if (std::fread(id, 1, 4, f) != 4 || std::fread(&size, 4, 1, f) != 1) break;
    const long next = std::ftell(f) + size;
    if (std::memcmp(id, "fmt ", 4) == 0) {
      uint16_t fmtTag = 0, ch = 0, b = 0;
      uint32_t sr = 0;
      std::fread(&fmtTag, 2, 1, f); std::fread(&ch, 2, 1, f);
      std::fread(&sr, 4, 1, f);
      std::fseek(f, 6, SEEK_CUR);  // byteRate + blockAlign
      std::fread(&b, 2, 1, f);
      channels = ch; fileRate = sr; bits = b;
    } else if (std::memcmp(id, "data", 4) == 0) {
      pcm.resize(size / 2);
      std::fread(pcm.data(), 1, size, f);
    }
    std::fseek(f, next, SEEK_SET);
  }
  std::fclose(f);
  if (rate) *rate = fileRate;
  out.reserve(pcm.size());
  (void)bits; (void)channels;
  for (int16_t v : pcm) out.push_back(v);
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string mode = argc > 1 ? argv[1] : "verify";
  int issues = 0;
  if (mode == "verify") {
    for (const Sample& s : kSamples) {
      int rate = 0;
      const auto vals = readWavRaw(std::string("Samples_src/") + s.name + ".wav", &rate);
      int mismatch = 0;
      if (rate != kRate || static_cast<int>(vals.size()) != s.len) mismatch = -1;
      else {
        for (int i = 0; i < s.len && !mismatch; ++i)
          if (vals[i] != s.data[i]) mismatch = i + 1;
      }
      if (mismatch) {
        std::printf("%-10s VERIFY FAIL (len/rate or sample@%d)\n", s.name, -mismatch);
        ++issues;
      }
    }
    if (issues) {
      std::printf("verify: %d FAILURES\n", issues);
    } else {
      std::printf("verify: all %u wavs byte-exact vs headers\n",
                  static_cast<unsigned>(std::size(kSamples)));
    }
    if (issues) return 1;
  } else if (mode == "verifygen") {
    // kSamples here holds the GENERATED values (this binary is compiled with
    // the workspace -I first). Each header must byte-equal the wav gen_headers
    // wrote it from: loops/<name>.wav when it exists, else <name>.wav.
    int checked = 0;
    for (const Sample& s : kSamples) {
      const std::string loopPath =
          std::string("Samples_src/loops/") + s.name + ".wav";
      std::string use = loopPath;
      std::error_code ec;
      if (!std::filesystem::exists(loopPath, ec))
        use = std::string("Samples_src/") + s.name + ".wav";
      int rate = 0;
      const auto vals = readWavRaw(use, &rate);
      int mismatch = 0;
      if (rate != kRate || static_cast<int>(vals.size()) != s.len) mismatch = -1;
      else {
        for (int i = 0; i < s.len && !mismatch; ++i)
          if (vals[i] != s.data[i]) mismatch = i + 1;
      }
      if (mismatch) {
        std::printf("%-10s VERIFYGEN FAIL vs %s (len/rate or sample@%d)\n",
                    s.name, use.c_str(), mismatch == -1 ? -1 : mismatch - 1);
        ++issues;
      }
      ++checked;
    }
    if (issues) {
      std::printf("verifygen: %d FAILURES of %d checked\n", issues, checked);
    } else {
      std::printf("verifygen: all %d generated headers byte-exact vs PCM\n", checked);
    }
    if (issues) return 1;
  } else {
    std::printf("usage: make_samples verify|verifygen\n");
    return 2;
  }
  return issues ? 1 : 0;
}
