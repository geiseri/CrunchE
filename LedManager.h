#ifndef LedManager_h
#define LedManager_h
#include <cstdint>
#include "InputManager.h"  // LedCommand enum: input -> LED strip interface
class LedManager {
public:
  int outPinA = 0;
  int outPinB = 0;
  int outPinC = 0;
  int outPinD = 0;
  float timeLit = 0;
  // Wall clock for metronome decay; unsigned so millis() wrap and long uptimes
  // keep 1 ms resolution (float loses that after ~4.7 h).
  unsigned long lastMillis = 0;
  // Pattern blink runs at wall-clock rate (~4 Hz), NOT per audio frame:
  // toggling every 45 us loop iteration would duty-cycle the LED to a dim
  // solid look (the "song mode sits there with LEDs solid" failure).
  unsigned long lastBlinkMillis = 0;
  LedCommand command = LedCommand::Applied;
  bool flipBlink = false;

  LedManager(int pinA, int pinB, int pinC, int pinD);
  void UpdateLed();
  void SetCommand(LedCommand command);
  void SetLit(float time, int col);
  // Voice-activity display: every triggered track LED joins a slow blink
  // cycle (1 s on / 0.5 s off) for as long as its notes keep replaying.
  void SetLitMask(uint8_t mask);
  // True when no metronome pulse or blink cycle owns the strip.
  bool isIdle() const;
  void SetPattern(bool patternPlay, int pattern);

private:
  int pattern = 0;
  bool patternPlay = false;
  bool patternPlayWas = false;  // edge detect for blink (re)start
  int litCol = -1;              // metronome column while timeLit > 0

  // Slow-blink cycle per track LED, driven by SetLitMask triggers.
  struct VoiceBlink {
    bool active = false;
    bool on = false;
    unsigned long cycleStart = 0;
    unsigned long lastTrig = 0;
  } blink[4];

  static constexpr unsigned long kBlinkOnMs = 1000;   // 1 s on
  static constexpr unsigned long kBlinkOffMs = 500;   // 0.5 s off
  // No trigger for a full on+off cycle -> the voice stopped replaying.
  static constexpr unsigned long kBlinkIdleMs = kBlinkOnMs + kBlinkOffMs + 100;

  void writePin(int i, int level);
  // Restore GPIOs from blink[] / metronome after arm hold ends.
  void resyncPins();
};
#endif
