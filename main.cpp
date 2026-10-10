#include <Arduino.h>
#include <Keypad.h>
#include <ESP_I2S.h>
#include <FastLED.h>
#include <algorithm>

#include "AudioConfig.h"
#include "OutputMixer.h"
#include "LedManager.h"
#include "InputManager.h"
#include "Tracker.h"

// Pins come from platformio.ini build_flags (PIN_*).

LedManager ledManager(PIN_LED_A, PIN_LED_B, PIN_LED_C, PIN_LED_D);
InputManager inputManager;
Tracker tracker;

// Keypad: silkscreen F1-F4 / G#-B / E-G / C-D#; matrix L1-L4 × R4-R1.
constexpr byte kKeypadRows = 4;
constexpr byte kKeypadCols = 4;

char keys[kKeypadRows][kKeypadCols] = {
  { 'A', 'B', 'C', 'D' },
  { 'E', 'F', 'G', 'H' },
  { 'I', 'J', 'K', 'L' },
  { 'M', 'N', 'O', 'P' }
};

byte rowPins[kKeypadRows] = {
  PIN_KEYPAD_ROW0, PIN_KEYPAD_ROW1, PIN_KEYPAD_ROW2, PIN_KEYPAD_ROW3
};
byte colPins[kKeypadCols] = {
  PIN_KEYPAD_COL0, PIN_KEYPAD_COL1, PIN_KEYPAD_COL2, PIN_KEYPAD_COL3
};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, kKeypadRows, kKeypadCols);

I2SClass I2S;
const int sampleRate = static_cast<int>(kAudioSampleRate);

constexpr int kNumLeds = 1;
constexpr uint32_t kLedUpdateIntervalMs = 33;  // ~30 fps; FastLED.show() blocks
constexpr int kVuPeakFullScale = 8000;         // post-/4 mix ceiling for brightness map
constexpr int kVuPeakDivisor = 32;             // maps peak → 0-250 brightness

CRGB leds[kNumLeds];
int ledPeak = 0;
uint8_t ledBrightness = 0;
uint32_t lastLedUpdate = 0;

// Soft ~2 s Cmaj9 swell through OutputMixer. Voices are static: each embeds
// an 8 KB history buffer; a local array would overflow the Arduino task stack.
static void playBootJingle() {
  struct Part {
    int instrument;
    int note;
    int octave;
  } parts[4] = {
      {7, 0, 1},    // jbass1  - C
      {4, 4, 1},    // pad1    - E
      {8, 7, 2},    // synth2  - G
      {13, 11, 2},  // guitar1 - B
  };
  static Voice voices[4];
  constexpr int kRate = static_cast<int>(kAudioSampleRate);
  const uint32_t total = kRate * 2;           // 2 s
  const uint32_t attack = kRate / 7;          // ~140 ms swell per entry
  const uint32_t stagger = kRate / 9;         // ~110 ms between voices
  const uint32_t release = kRate * 3 / 10;    // final 300 ms fade

  // Let amp/SD_MODE settle before the swell so the power-on transient is silent.
  const int16_t silence = 0;
  for (uint32_t frame = 0; frame < kRate * 12 / 100; frame++) {  // ~120 ms
    I2S.write(reinterpret_cast<const uint8_t *>(&silence), sizeof(silence));
  }

  for (int voiceIndex = 0; voiceIndex < 4; voiceIndex++) {
    voices[voiceIndex].SetVolume(1);
    voices[voiceIndex].SetEnvelopeNum(2);              // sustain shape
    voices[voiceIndex].SetEnvelopeLength(480000);      // outlives the jingle
    voices[voiceIndex].SetNote(parts[voiceIndex].note, false,
                               parts[voiceIndex].octave,
                               parts[voiceIndex].instrument);
  }

  for (uint32_t frame = 0; frame < total; frame++) {
    int32_t mix = 0;
    for (int voiceIndex = 0; voiceIndex < 4; voiceIndex++) {
      const uint32_t start = static_cast<uint32_t>(voiceIndex) * stagger;
      float env = 1.0f;
      if (frame < start) {
        env = 0.0f;
      } else if (frame < start + attack) {
        env = static_cast<float>(frame - start) / static_cast<float>(attack);
      }
      if (frame + release > total) {
        env *= static_cast<float>(total - frame) / static_cast<float>(release);
      }
      // Float envelope stay continuous; quantize once into the mix sum.
      const float wet =
          static_cast<float>(voices[voiceIndex].UpdateVoice()) * env * 0.5f;
      mix += static_cast<int32_t>(wet);
    }
    const int16_t outputSample = StageMasterSample(mix);
    I2S.write(reinterpret_cast<const uint8_t *>(&outputSample),
              sizeof(outputSample));
  }
}


static void updateNeoPixelVu() {
  const uint32_t now = millis();
  if (now - lastLedUpdate < kLedUpdateIntervalMs) {
    return;
  }
  lastLedUpdate = now;

  // Pattern 2 is bass-only: VU follows the bass voice peak for mixing diagnostics.
  const int diagPeak =
      tracker.currentPattern == 2 ? std::max(ledPeak, tracker.voicePeak[1])
                                  : ledPeak;
  const uint8_t instant = static_cast<uint8_t>(
      std::min(diagPeak, kVuPeakFullScale) / kVuPeakDivisor);
  ledBrightness = static_cast<uint8_t>((3 * ledBrightness + instant) / 4);
  ledPeak = 0;

  // Hue = track color + pattern nudge; brightness = loudness.
  const uint8_t trackHue = static_cast<uint8_t>(
      tracker.selectedTrack * 64 + tracker.currentPattern * 8);
  leds[0] = CHSV(trackHue, 255, ledBrightness);
  FastLED.show();

  tracker.voicePeak[0] = tracker.voicePeak[1] = tracker.voicePeak[2] =
      tracker.voicePeak[3] = 0;
}

static void updateStripLeds() {
  // Voice-activity mask wins; else metronome outside song mode; else pattern blink.
  if (tracker.lastTriggeredMask != 0) {
    ledManager.SetLitMask(tracker.lastTriggeredMask);
  } else if (tracker.tempoBlink > 0 && !tracker.allPatternPlay) {
    ledManager.SetLit(tracker.tempoBlink, tracker.selectedTrack);
  }

  const bool stripIdle = tracker.lastTriggeredMask == 0 &&
                         tracker.tempoBlink == 0 && ledManager.isIdle();
  ledManager.SetPattern(tracker.allPatternPlay && stripIdle,
                        tracker.currentPattern);
  ledManager.UpdateLed();
}

void setup() {
  FastLED.addLeds<NEOPIXEL, PIN_NEOPIXEL>(leds, kNumLeds);
  I2S.setPins(PIN_I2S_BCLK, PIN_I2S_WS, PIN_I2S_DOUT);

  if (!I2S.begin(I2S_MODE_STD, sampleRate, I2S_DATA_BIT_WIDTH_16BIT,
                 I2S_SLOT_MODE_MONO)) {
    Serial.println("Failed to initialize I2S!");
    while (true) {
    }
  }

  playBootJingle();
  // tracker.LoadDemoSong();  // boot with the demo loop playing
}

void loop() {
  inputManager.UpdateInput(keypad.getKey());

  if (inputManager.ledCommand != LedCommand::None) {
    ledManager.SetCommand(inputManager.ledCommand);
  }
  if (inputManager.trackCommand != Command::None) {
    tracker.SetCommand(inputManager.trackCommand,
                       inputManager.trackCommandArgument);
  }

  const int16_t outputSample = StageMasterSample(tracker.UpdateTracker());
  I2S.write(reinterpret_cast<const uint8_t *>(&outputSample),
            sizeof(outputSample));

  const int level = abs(outputSample);
  if (level > ledPeak) {
    ledPeak = level;
  }

  updateNeoPixelVu();
  updateStripLeds();
  inputManager.EndFrame();
}
