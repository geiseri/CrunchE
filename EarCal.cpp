#include "EarCal.h"

#include <Arduino.h>
#include <cmath>

#include "AudioConfig.h"
#include "EarGrid.h"
#include "OutputMixer.h"

namespace {

constexpr float kTwoPi = 6.28318530718f;
// Pre-limiter level; StageMasterSample + default trim keep the coil safe.
constexpr float kToneAmplitude = 12000.0f;
constexpr char kKeyF4 = 'P';

}  // namespace

void EarCal::begin(int ledA, int ledB, int ledC, int ledD) {
  ledA_ = ledA;
  ledB_ = ledB;
  ledC_ = ledC;
  ledD_ = ledD;
  phase_ = EarCalPhase::Lo;
  toneOn_ = false;
  phaseAccum_ = 0.0f;
  phaseStep_ = 0.0f;
  f4Down_ = false;
  f4LongHandled_ = false;
  updatePhaseLeds();
}

int EarCal::gridIndexForKey(char key) {
  // Keypad chars → silkscreen EarGrid order (see EarGrid.h / README).
  switch (key) {
    case 'M':
      return 0;  // F1
    case 'N':
      return 1;  // F2
    case 'O':
      return 2;  // F3
    case 'P':
      return 3;  // F4
    case 'I':
      return 4;  // G#
    case 'J':
      return 5;  // A
    case 'K':
      return 6;  // A#
    case 'L':
      return 7;  // B
    case 'E':
      return 8;  // E
    case 'F':
      return 9;  // F
    case 'G':
      return 10;  // F#
    case 'H':
      return 11;  // G
    case 'A':
      return 12;  // C
    case 'B':
      return 13;  // C#
    case 'C':
      return 14;  // D
    case 'D':
      return 15;  // D#
    default:
      return -1;
  }
}

void EarCal::setToneHz(float hz) {
  const float rate = static_cast<float>(kAudioSampleRate);
  phaseStep_ = kTwoPi * hz / rate;
  phaseAccum_ = 0.0f;
  toneOn_ = hz > 0.0f && hz < rate * 0.5f;
}

void EarCal::playKey(char key) {
  const int index = gridIndexForKey(key);
  if (index < 0) {
    return;
  }
  const float hz =
      (phase_ == EarCalPhase::Lo) ? kEarLoHz[index] : kEarHiHz[index];
  setToneHz(hz);
}

void EarCal::togglePhase() {
  phase_ = (phase_ == EarCalPhase::Lo) ? EarCalPhase::Hi : EarCalPhase::Lo;
  toneOn_ = false;
  phaseStep_ = 0.0f;
  updatePhaseLeds();
}

void EarCal::updatePhaseLeds() {
  if (ledA_ < 0) {
    return;
  }
  const bool lo = phase_ == EarCalPhase::Lo;
  digitalWrite(ledA_, lo ? HIGH : LOW);
  digitalWrite(ledB_, lo ? LOW : HIGH);
  digitalWrite(ledC_, LOW);
  digitalWrite(ledD_, LOW);
}

void EarCal::poll(Keypad &keypad) {
  if (!keypad.getKeys()) {
    return;
  }
  for (uint8_t i = 0; i < LIST_MAX; ++i) {
    const Key &entry = keypad.key[i];
    if (entry.kchar == NO_KEY || entry.stateChanged == false) {
      continue;
    }
    const char key = entry.kchar;
    const KeyState state = entry.kstate;

    if (key == kKeyF4) {
      if (state == PRESSED) {
        f4Down_ = true;
        f4DownMs_ = millis();
        f4LongHandled_ = false;
      } else if (state == HOLD && f4Down_ && !f4LongHandled_) {
        if (millis() - f4DownMs_ >= kEarCalLongPressMs) {
          togglePhase();
          f4LongHandled_ = true;
        }
      } else if (state == RELEASED) {
        if (f4Down_ && !f4LongHandled_) {
          playKey(kKeyF4);
        }
        f4Down_ = false;
        f4LongHandled_ = false;
      }
      continue;
    }

    if (state == PRESSED) {
      playKey(key);
    }
  }

  // HOLD events may not set stateChanged every frame; re-check F4 duration.
  if (f4Down_ && !f4LongHandled_ && keypad.isPressed(kKeyF4)) {
    if (millis() - f4DownMs_ >= kEarCalLongPressMs) {
      togglePhase();
      f4LongHandled_ = true;
    }
  }
}

int16_t EarCal::nextSample() {
  if (!toneOn_) {
    return StageMasterSample(0);
  }
  const float sample = sinf(phaseAccum_) * kToneAmplitude;
  phaseAccum_ += phaseStep_;
  if (phaseAccum_ >= kTwoPi) {
    phaseAccum_ -= kTwoPi;
  }
  return StageMasterSample(static_cast<int>(sample));
}

bool UpdateEarCalBootHold(Keypad &keypad, uint32_t &heldMs, uint32_t &lastMs) {
  const uint32_t now = millis();
  if (lastMs == 0) {
    lastMs = now;
  }
  const uint32_t delta = now - lastMs;
  lastMs = now;

  keypad.getKeys();
  if (keypad.isPressed(kKeyF4)) {
    heldMs += delta;
  } else {
    heldMs = 0;
  }
  return heldMs >= kEarCalBootHoldMs;
}
