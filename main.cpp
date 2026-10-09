#include <Arduino.h>
#include "AudioConfig.h"
#include "OutputMixer.h"

#include "LedManager.h"
LedManager ledManager = LedManager(1, 2, 4, 8);

#include "InputManager.h"
InputManager inputManager = InputManager();

//tracker
#include "Tracker.h"
Tracker tracker = Tracker();

//Matrix keypad library
#include <Keypad.h>
const byte ROWS = 4;
const byte COLS = 4;

/* Silkscreen
F1 F2 F3 F4
G# A  A# B
E  F  F# G
C  C# D  D#
*/

/* Matrix
L1,R4 L2,R4 L3,R4 L4,R4
L1,R3 L2,R3 L3,R3 L4,R3
L1,R2 L2,R2 L3,R2 L4,R2
L1,R1 L2,R1 L3,R1 L4,R1
*/

/*
pins L1 L2 L3 L4 R1 R2 R3 R4
*/

char keys[ROWS][COLS] = {
  { 'A', 'B', 'C', 'D' },
  { 'E', 'F', 'G', 'H' },
  { 'I', 'J', 'K', 'L' },
  { 'M', 'N', 'O', 'P' }
};

byte rowPins[ROWS] = { 13, 3, 44, 43 };     //connect to the row pinouts of the keypad
byte colPins[COLS] = { 9, 10, 11, 12 };  //connect to the column pinouts of the keypad
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

char oldChar;

// i2s sound
#include <ESP_I2S.h>
I2SClass I2S;
const int sampleRate = static_cast<int>(kAudioSampleRate);


#include <FastLED.h>
#include <algorithm>

#define NUM_LEDS 1
#define DATA_PIN 48

CRGB leds[NUM_LEDS];

// LED VU state: peak of |sample| since last refresh, smoothed brightness
int ledPeak = 0;
uint8_t ledBrightness = 0;
uint32_t lastLedUpdate = 0;



// Boot greeting: a soft ~2 s Cmaj9 swell across four dedicated Voices,
// summed and routed through the real OutputMixer chain. Staggered attack
// fades give the power-on moment a musical rising edge instead of the
// bare "snap", and the release fade prevents a cutoff click. Local Voice
// objects (not the tracker's) keep user state completely untouched - but
// NOTE: each Voice embeds an 8 KB history buffer, so these MUST be static
// (a local array of four would overflow the 8 KB Arduino task stack).
static void playBootJingle() {
  struct Part { int instrument; int note; int octave; } parts[4] = {
      {7, 0, 1},    // jbass1  - C
      {4, 4, 1},    // pad1    - E
      {8, 7, 2},    // synth2  - G
      {13, 11, 2},  // guitar1 - B
  };
  static Voice voices[4];
  constexpr int kRate = static_cast<int>(kAudioSampleRate);
  const uint32_t total = kRate * 2;           // 2 s @ 22050
  const uint32_t attack = kRate / 7;          // ~140 ms swell per entry
  const uint32_t stagger = kRate / 9;         // ~110 ms between voices
  const uint32_t release = kRate * 3 / 10;    // final 300 ms fade

  // Lead-in silence: let the amp/SD_MODE rail settle so its power-on
  // transient lands against nothing, before the swell starts.
  const int16_t silence = 0;
  for (uint32_t n = 0; n < kRate * 12 / 100; n++) {  // ~120 ms
    I2S.write(reinterpret_cast<const uint8_t *>(&silence), sizeof(silence));
  }

  for (int i = 0; i < 4; i++) {
    voices[i].SetVolume(1);
    voices[i].SetEnvelopeNum(2);              // sustain shape
    voices[i].SetEnvelopeLength(480000);      // outlives the jingle
    voices[i].SetNote(parts[i].note, false, parts[i].octave,
                      parts[i].instrument);
  }

  for (uint32_t n = 0; n < total; n++) {
    int mix = 0;
    for (int i = 0; i < 4; i++) {
      uint32_t start = static_cast<uint32_t>(i) * stagger;
      float env = 1.0f;
      if (n < start) {
        env = 0.0f;
      } else if (n < start + attack) {
        env = static_cast<float>(n - start) / attack;
      }
      if (n + release > total) {
        env *= static_cast<float>(total - n) / release;
      }
      mix += static_cast<int>(voices[i].UpdateVoice() * env * 0.5f);
    }
    const int16_t outputSample = StageMasterSample(mix);
    I2S.write(reinterpret_cast<const uint8_t *>(&outputSample),
              sizeof(outputSample));
  }
}

void setup() {
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  // setup I2S pins: bclk, ws, dout, din, mclk
  I2S.setPins(6, 5, 7);

  if (!I2S.begin(I2S_MODE_STD, sampleRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("Failed to initialize I2S!");
    while (1)
      ;
  }

  playBootJingle();

  //tracker.LoadDemoSong();  // boot with the demo loop playing
}

void loop() {
  inputManager.UpdateInput(keypad.getKey());
  Command trackCommand = inputManager.trackCommand;
  int trackCommandArgument = inputManager.trackCommandArgument;
  LedCommand ledCommand = inputManager.ledCommand;

  if (ledCommand != LedCommand::None) {
    ledManager.SetCommand(ledCommand);
  }

  if (trackCommand != Command::None) {
    tracker.SetCommand(trackCommand, trackCommandArgument);
  }

  // Master gain/limit + 16-bit staging, shared with the native test harness.
  const int16_t outputSample = StageMasterSample(tracker.UpdateTracker());
  I2S.write(reinterpret_cast<const uint8_t *>(&outputSample), sizeof(outputSample));

  // Peak-hold the post-staging level while audio runs.
  const int sample = outputSample;
  if (abs(sample) > ledPeak) {
    ledPeak = abs(sample);
  }

  // Refresh the NeoPixel at ~30 fps; FastLED.show() blocks for the whole
  // WS2812 frame and would starve the 22 kHz audio loop if called per sample.
  const uint32_t now = millis();
  if (now - lastLedUpdate >= 33) {
    lastLedUpdate = now;
    // Diagnostic readout: on the bass-only pattern (2), the RGB VU is driven
    // by the bass voice's own sample peak. RGB pulsing in sync with LED B =
    // the bass signal exists but is too quiet/off-pitch to hear (mixing
    // problem). RGB dark = the voice outputs silence (code problem).
    // Everywhere else the RGB follows the master level as usual.
    const int diagPeak =
        tracker.currentPattern == 2 ? std::max(ledPeak, tracker.voicePeak[1])
                                    : ledPeak;
    // Map peak to 0-250 brightness against the post-/4 signal (single voice
    // tops out near 2000, full mix near 8000), then smooth so the LED falls
    // gently instead of flickering per frame.
    const uint8_t instant = static_cast<uint8_t>(std::min(diagPeak, 8000) / 32);
    ledBrightness = static_cast<uint8_t>((3 * ledBrightness + instant) / 4);
    ledPeak = 0;

    // Color follows the tracker: base hue picks the selected voice
    // (red / yellow / green / blue), nudged 8 steps per pattern so pattern
    // switches are visible without losing the track identity, and loudness
    // sets the brightness. Max hue is 3*64 + 3*8 = 216, no uint8 overflow.
    const uint8_t trackHue =
        static_cast<uint8_t>(tracker.selectedTrack * 64 + tracker.currentPattern * 8);
    leds[0] = CHSV(trackHue, 255, ledBrightness);
    FastLED.show();

    // Per-voice peaks are consumed by the readout above every tick.
    tracker.voicePeak[0] = tracker.voicePeak[1] = tracker.voicePeak[2] =
        tracker.voicePeak[3] = 0;
  }

  // Strip LEDs follow voice activity even in song mode: an LED firing with
  // no corresponding sound localizes the fault to the audio path (wiring,
  // amp, I2S) rather than the sequencer. While a voice's notes keep
  // replaying, its LED runs the slow blink (1 s on, 0.5 s off) instead of
  // strobing per step. The metronome blink covers idle playback outside
  // song mode, and the pattern blink only runs when the strip would
  // otherwise be dark, so the writers never fight over the same pins.
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
  inputManager.EndFrame();
}
