#include "Voice.h"
#include "InputManager.h"  // Command enum: keypad -> tracker interface
#include <cstdint>
#ifndef Tracker_h
#define Tracker_h

// Time seam: on the ESP32 build this is wired to millis() (see Tracker.cpp);
// native test builds provide a simulated clock instead, so Tracker/Voice can
// be exercised off-device.
namespace tracker_clock {
uint32_t Now();
}

class Tracker {
public:
  bool isPlaying;
  float tempoBlink;
  int selectedTrack;
  // Record session: the first note entered while playing opens it (notes
  // then land on the 16th grid - build alongside the other tracks). F4+F4
  // CLOSES it without stopping playback (that's "commit"); with no session
  // open, note keys audition live only and never touch the grid, so a
  // post-commit slip cannot scatter stray steps. Changing tracks or
  // stopping the transport commits implicitly.
  bool recording = false;
  bool allPatternPlay;
  int currentPattern;
  // Bit i set when track i triggered a note on the latest update frame.
  uint8_t lastTriggeredMask = 0;
  // Instrument bank: F4+D toggles. Bank 0 selects voiceNum 0-11 (drums,
  // sfx, originals); bank 1 maps F1+note to voiceNum 12-23 (added samples;
  // 21-23 are unprovisioned slots that play silence).
  int instrumentBank = 0;
  // Diagnostic: peak |sample| each voice generated since last reset. A voice
  // whose LED blinks but whose peak reads 0 is silent in code, not hardware.
  int voicePeak[4] = {0, 0, 0, 0};
  Tracker();
  int UpdateTracker();
  void SetCommand(Command command, int val);
  void LoadDemoSong();
  // Read-only audit hooks for the native gates (multi-note recording,
  // commit semantics). Production code never calls these.
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
  int trackIndex = 0;
  uint32_t lastMillis = 0;
  int patternLength = 32;
  
  Voice voices[4];
  float soundVelocity;
  int currentVoice=0;

  int bpms[4];
  int tracks[4][256];
  int trackOctaves[4][256];
  int trackInstruments[4][256];
  int patternCopy[4][64];
  int patternCopyOctaves[4][64];
  int patternCopyInstruments[4][64];
 
  void SetNote(int val,int track);
  void SetArp(int val);
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
  int UpdateVoices();
  int heldNotes[4];
  int heldInsturments[4];
};

#endif