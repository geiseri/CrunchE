#include "Voice.h"
#include "InputManager.h"  // Command enum: keypad -> tracker interface
#include <array>
#include <cstdint>
#include <span>
#ifndef Tracker_h
#define Tracker_h

constexpr uint8_t kTrackCount = 4;
constexpr uint16_t kGridLength = 256;
constexpr uint8_t kPatternClipboardLength = 64;
constexpr uint8_t kPatternCount = 4;

// Latched step event: filled on the millis clock edge, applied on the next
// audio sample frame (UpdateTracker). Quantize write uses trackIndex; play
// uses trackIndex - 1 (one-behind cursor).
struct PendingTrigger {
  uint8_t track = 0;
  int8_t note = 0;       // chromatic 0-11 (cell stored note+1)
  int8_t octave = 0;
  uint8_t instrument = 0;
  bool isDelay = false;  // Tracker step-delay echo, not EffectMode sample FX
};

// Time seam: on the ESP32 build this is wired to millis() (see Tracker.cpp);
// native test builds provide a simulated clock instead, so Tracker/Voice can
// be exercised off-device.
namespace tracker_clock {
uint32_t Now();
}

class Tracker {
public:
  bool isPlaying = false;
  float tempoBlink = 0;
  int selectedTrack = 0;
  // Record session: the first note entered while playing opens it (notes
  // then land on the 16th grid - build alongside the other tracks). F4+F4
  // CLOSES it without stopping playback (that's "commit"); with no session
  // open, note keys audition live only and never touch the grid, so a
  // post-commit slip cannot scatter stray steps. Changing tracks or
  // stopping the transport commits implicitly.
  bool recording = false;
  bool allPatternPlay = false;
  int currentPattern = 0;
  // Bit i set when track i triggered a note on the latest update frame.
  uint8_t lastTriggeredMask = 0;
  // Instrument bank: F4+D toggles. Bank 0 selects voiceNum 0-11 (drums,
  // sfx, originals); bank 1 maps F1+note to voiceNum
  // kInstrumentBank1Offset..(offset+11) (KeypadConstants.h; top slots may
  // be silent if unprovisioned in Voice.cpp).
  int instrumentBank = 0;
  // Diagnostic: peak |sample| each voice generated since last reset. A voice
  // whose LED blinks but whose peak reads 0 is silent in code, not hardware.
  int voicePeak[kTrackCount] = {0, 0, 0, 0};
  Tracker();
  [[nodiscard]] int UpdateTracker();
  void SetCommand(Command command, int val);
  void LoadDemoSong();
  // Read-only audit hooks for the native gates (multi-note recording,
  // commit semantics). Production code never calls these. Widen narrow
  // grid cells to int for simple native asserts.
  int CellAt(int track, int step) const { return tracks[track][step]; }
  int CellOctave(int track, int step) const {
    return trackOctaves[track][step];
  }
  int CellInstrument(int track, int step) const {
    return trackInstruments[track][step];
  }
 
private:
  float bpm = 0;
  float bps = 0;
  float beatTime = 0;
  float noteTime = 0;
  int barCount = 0;
  uint16_t trackIndex = 0;
  uint32_t lastMillis = 0;
  int patternLength = 32;
  
  Voice voices[kTrackCount];
  float soundVelocity;
  int currentVoice = 0;

  int bpms[kTrackCount];
  int8_t tracks[kTrackCount][kGridLength];
  int8_t trackOctaves[kTrackCount][kGridLength];
  uint8_t trackInstruments[kTrackCount][kGridLength];
  int8_t patternCopy[kTrackCount][kPatternClipboardLength];
  int8_t patternCopyOctaves[kTrackCount][kPatternClipboardLength];
  uint8_t patternCopyInstruments[kTrackCount][kPatternClipboardLength];

  // Hybrid clock: Phase A latches here; Phase B consumes on the sample edge.
  std::array<PendingTrigger, kTrackCount> pendingTriggers_{};
  uint8_t pendingCount_ = 0;

  void SetNote(int val, int track);
  void SetArp(int val);  // Command::Arp -> EffectMode (sample FX)
  void SetBPM(int val);
  void SetDelay(int val);
  void SetEnvelopeNum(int val);
  void SetVoice(int val);
  void SetEnvelopeLength(int val);
  void SetOctave(int val);
  void SetVolume(int val);
  void SetTrackNum(int val);
  void ClearTrackNum(int val);
  void SetPatternNum(int val);
  void ClearPatternNum(int val);
  void TogglePlayStop();
  void CopyTrack();
  void PasteTrack();
  void CopyPattern();
  void PastePattern();
  void SetPatternLength(int val);
  void SaveDefaultSong();
  void WriteDemoStep(int track, int step, int note, int octave, int instrument);
  void ClearAll(int val);
  void LatchStepTriggers(int step, int patStart);
  void ApplyPendingTriggers(std::span<const PendingTrigger> pending);
};

#endif