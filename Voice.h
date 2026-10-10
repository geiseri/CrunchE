#ifndef Voice_h
#define Voice_h

#include "AudioConfig.h"

#include <cstdint>
#include <span>

// Instrument bank family: drums/sfx are one-shots; pitched voices loop.
enum class InstrumentFamily : uint8_t {
  Drum = 0,
  Sfx = 1,
  Pitched = 2,
};

// Per-voice sample FX (keypad Command::Arp selects these; not a melodic arp).
enum class EffectMode : uint8_t {
  Off = 0,
  Lowpass8 = 1,
  Lowpass4 = 2,
  Echo = 3,
};

// voiceNum: 0 = drums, 1 = sfx, 2+ = pitched instrument bank slots.
[[nodiscard]] constexpr InstrumentFamily FamilyOf(int voiceNum) {
  if (voiceNum == 0) {
    return InstrumentFamily::Drum;
  }
  if (voiceNum == 1) {
    return InstrumentFamily::Sfx;
  }
  return InstrumentFamily::Pitched;
}

// UpdateVoice is one sample at kAudioSampleRate; envelope countdown uses
// this fixed decrement (no runtime sample-rate switching).
constexpr int32_t kEnvelopeDecrementPerSample = 10;

constexpr int kSampleHistoryLength = 2000;

class Voice {
public:
  // Step-grid echo depth (Tracker retriggers older cells). Distinct from
  // EffectMode::Echo, which is a sample-history FX tap.
  int delay = 0;
  int octave = 0;

  Voice() = default;
  [[nodiscard]] int32_t UpdateVoice();

  void SetNote(int val, bool delay, int optOctave, int optInstrument);
  void SetVolume(int val);
  void SetOctave(int val);
  void SetDelay(int val);
  void SetEnvelopeNum(int val);
  void SetEnvelopeLength(int val);
  void SetEffectMode(EffectMode mode);
  // Keypad Command::Arp maps here (FX mode, not a melodic arpeggio).
  void SetArpNum(int val) { SetEffectMode(static_cast<EffectMode>(val)); }
  // Read-only audit for native gates (playhead reset / family change).
  [[nodiscard]] float SampleIndexForTest() const { return sampleIndex_; }

private:
  EffectMode effectMode_ = EffectMode::Off;
  bool isDelay_ = false;
  int32_t envelopeLength_ = 60000;
  int32_t envelope_ = 0;
  int envelopeNum_ = 0;
  int voiceNum_ = 0;
  int note_ = 0;
  // History taps stay within ~±9000 by the voice loudness contract.
  int16_t sampleHistory_[kSampleHistoryLength] = {};
  int sampleHistoryIndex_ = 0;

  float baseFreq_ = 1.0f;
  float sampleIndex_ = 0.0f;
  float volume_ = 1.0f;

  [[nodiscard]] int32_t ReadWaveform();
  [[nodiscard]] int32_t ReadDrumWaveform();
  [[nodiscard]] int32_t ReadSfxWaveform();
  [[nodiscard]] int32_t ReadOneShotWaveform(std::span<const int> source);
  float GetBaseFreq(int val, int ioctave);
  float GetVolumeRatio();
  float LerpSample(int32_t sampleA, int32_t sampleB, float ratio);
  void UpdateHistory(int32_t sample);
  int32_t GetHistorySample(int backOffset);
};
#endif
