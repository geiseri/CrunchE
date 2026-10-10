#pragma once

#include <Keypad.h>
#include <cstdint>

// Boot-time speaker ear calibration: on-device sines for the lo/hi grids in
// EarGrid.h. Bypasses InputManager / Tracker. Reboot to exit.

constexpr uint32_t kEarCalBootHoldMs = 500;
constexpr uint32_t kEarCalLongPressMs = 700;
// Poll keypad every ~10 ms while the blocking boot jingle runs.
constexpr uint32_t kEarCalBootPollSamples = 220;

enum class EarCalPhase : uint8_t { Lo = 0, Hi = 1 };

class EarCal {
 public:
  void begin(int ledA, int ledB, int ledC, int ledD);
  // Scan keypad: short keys start tones; long F4 toggles lo/hi phase.
  void poll(Keypad &keypad);
  int16_t nextSample();
  void updatePhaseLeds();
  EarCalPhase phase() const { return phase_; }

 private:
  void setToneHz(float hz);
  void playKey(char key);
  void togglePhase();
  static int gridIndexForKey(char key);

  EarCalPhase phase_ = EarCalPhase::Lo;
  int ledA_ = -1;
  int ledB_ = -1;
  int ledC_ = -1;
  int ledD_ = -1;

  float phaseAccum_ = 0.0f;
  float phaseStep_ = 0.0f;
  bool toneOn_ = false;

  bool f4Down_ = false;
  uint32_t f4DownMs_ = 0;
  bool f4LongHandled_ = false;
};

// Accumulate continuous F4 hold during the boot jingle. Call with getKeys()
// already updating state each poll. Returns true once hold >= kEarCalBootHoldMs.
bool UpdateEarCalBootHold(Keypad &keypad, uint32_t &heldMs, uint32_t &lastMs);
