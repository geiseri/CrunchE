#include "Tracker.h"
#include "OutputMixer.h"
#include "Voice.h"

#ifdef ARDUINO
#include <Arduino.h>
namespace tracker_clock {
uint32_t Now() { return millis(); }
} // namespace tracker_clock
#endif

Tracker::Tracker() {
  patternLength = 32;
  isPlaying = true;
  lastMillis = tracker_clock::Now();
  bpms[0] = 120;
  bpms[1] = 132;
  bpms[2] = 145;
  bpms[3] = 180;
  SetBPM(0);

  for (int track = 0; track < kTrackCount; track++) {
    for (int step = 0; step < kGridLength; step++) {
      tracks[track][step] = 0;
    }
  }
  ClearAll(0);
}

void Tracker::ApplyPendingTriggers(std::span<const PendingTrigger> pending) {
  for (const PendingTrigger &trigger : pending) {
    voices[trigger.track].SetNote(trigger.note, trigger.isDelay, trigger.octave,
                                  trigger.instrument);
    lastTriggeredMask |= static_cast<uint8_t>(1u << trigger.track);
  }
}

void Tracker::LatchStepTriggers(int step, int patStart) {
  // Cell values are note+1 so 0 can mean rest. Step-delay (voice.delay)
  // retriggers an older cell; that is not EffectMode sample FX.
  pendingCount_ = 0;
  for (uint8_t track = 0; track < kTrackCount; track++) {
    const int8_t note = tracks[track][step];
    if (note > 0) {
      pendingTriggers_[pendingCount_++] = PendingTrigger{
          .track = track,
          .note = static_cast<int8_t>(note - 1),
          .octave = trackOctaves[track][step],
          .instrument = trackInstruments[track][step],
          .isDelay = false,
      };
    } else if (const int delay = voices[track].delay; delay > 0) {
      int echoStep = step - delay;
      if (echoStep < patStart) {
        echoStep += patternLength;
      }
      const int8_t echoNote = tracks[track][echoStep];
      if (echoNote > 0) {
        // Replay the echo cell exactly as recorded, including its stored
        // octave. The previous -1 sentinel fell back to the voice's
        // octave member (0 unless the keypad changed it), so demo echoes
        // played two octaves under the arp and beat against it as buzz.
        pendingTriggers_[pendingCount_++] = PendingTrigger{
            .track = track,
            .note = static_cast<int8_t>(echoNote - 1),
            .octave = trackOctaves[track][echoStep],
            .instrument = trackInstruments[track][echoStep],
            .isDelay = true,
        };
      }
    }
  }
}

int Tracker::UpdateTracker() {
  // Hybrid timing model (DJ handheld):
  //   Phase B — apply any triggers latched on a prior clock edge so onsets
  //             align to the audio sample stream (one UpdateTracker = one
  //             sample on device).
  //   Phase A — accumulate millis*BPM; when a 16th is due, latch cells into
  //             pendingTriggers_ (do not SetNote yet).
  // Tempo authority stays wall-clock (tracker_clock::Now / bps). Trigger
  // authority is the sample edge. Quantize write uses trackIndex; play uses
  // trackIndex - 1 (one-behind).
  constexpr float kBeatUnits = 1000.0f;
  constexpr float kStepUnits = kBeatUnits / 4.0f;
  constexpr int kStepsPerBar = 8;

  // Phase B: consume latched triggers on this sample edge.
  lastTriggeredMask = 0;
  if (pendingCount_ > 0) {
    ApplyPendingTriggers(std::span<const PendingTrigger>(
        pendingTriggers_.data(), pendingCount_));
    pendingCount_ = 0;
  }

  const uint32_t now = tracker_clock::Now();
  float delta = static_cast<float>(
      now - lastMillis); // wraps cleanly at millis() rollover
  lastMillis = now;
  if (!isPlaying) {
    delta = 0.0f;
  }

  tempoBlink = 0;
  beatTime += delta * bps;
  noteTime += delta * bps;

  if (beatTime >= kBeatUnits) {
    beatTime -= kBeatUnits;
    barCount = (barCount + 1) % kStepsPerBar;
    tempoBlink = (barCount == 0) ? 100 : 20; // long pulse on the downbeat
  }

  // Phase A: latch step events when a 16th crosses (applied next sample).
  if (noteTime >= kStepUnits) {
    noteTime -= kStepUnits;
    trackIndex++;

    // Wrap within the current pattern, or advance through all patterns in
    // song mode. patStart..patEnd always stays inside tracks[256] because
    // patternLength is 32 or 64 and there are 4 patterns.
    if (trackIndex >= patternLength * (currentPattern + 1)) {
      if (allPatternPlay) {
        currentPattern = (currentPattern + 1) % kPatternCount;
      }
      trackIndex = patternLength * currentPattern;
    }
    const int patStart = patternLength * currentPattern;

    // Play one step behind the write cursor so live input lands on the next
    // pass; at the first step, reach back to the pattern's last step.
    int step = static_cast<int>(trackIndex) - 1;
    if (step < patStart) {
      step = patStart + patternLength - 1;
    }

    LatchStepTriggers(step, patStart);
  }

  int32_t masterSample = 0;
  for (uint8_t track = 0; track < kTrackCount; track++) {
    const int32_t voiceSample = voices[track].UpdateVoice();
    const int32_t magnitude = voiceSample < 0 ? -voiceSample : voiceSample;
    if (magnitude > voicePeak[track]) {
      voicePeak[track] = static_cast<int>(magnitude);
    }
    masterSample += voiceSample;
  }
  return static_cast<int>(masterSample);
}

void Tracker::WriteDemoStep(int track, int step, int note, int octave,
                            int instrument) {
  if (track < 0 || track > 3 || step < 0 || step >= patternLength * 4 ||
      note < 0 || note > 11) {
    return;
  }
  tracks[track][step] =
      static_cast<int8_t>(note + 1); // cells store note+1 so 0 is a rest
  trackOctaves[track][step] = static_cast<int8_t>(octave);
  trackInstruments[track][step] = static_cast<uint8_t>(instrument);
}

void Tracker::LoadDemoSong() {
  // Am - F - G - C synthpop loop; 32 steps per pattern at 120 BPM = two
  // bars of sixteenths. Track 0 drums, 1 bass, 2 echo arp lead, 3 pad.
  //
  // The demo doubles as a manual mix diagnostic, one pattern per stem:
  //   pattern 0: full mix
  //   pattern 1: drums alone
  //   pattern 2: bass alone
  //   pattern 3: lead + pad alone
  // Song mode is left ON so playback auto-cycles mix -> stems; select
  // patterns by hand with F3 + E/F/F#/G. The strip LEDs track voice
  // activity: an LED firing with no sound localizes the fault to the
  // audio path (I2S wiring, amp, output), while a silent LED with no
  // sound is the sequencer/code.
  constexpr int kKick = 0, kSnare = 1, kHatClosed = 3, kHatOpen = 7, kClap = 9;
  constexpr int kInstDrums = 0, kInstBass = 9, kInstLead = 13, kInstPad = 4;
  // kInstLead: guitar1 - the WAV pipeline measured synth2's source itself
  // clicking ~183x/second at 1x (bursts baked into the recording; the
  // loop seam fix cost=0.001 did its job), while looped guitar1 renders the
  // demo arp at 0.0 clicks/s. synth2 remains selectable but is unsuited to
  // sustained lines on any speaker.

  ClearAll(0); // 32-step patterns, silent tracks, voice defaults
  SetBPM(0);   // 120 BPM
  currentPattern = 0;
  trackIndex = 0;
  selectedTrack = 0;
  isPlaying = true;
  allPatternPlay = true; // auto-cycle: mix, then each stem alone

  struct Step {
    int note;
    int oct;
  };

  // One chord per 8 steps (two beats); chromatic indices: 0=C, 5=F, 7=G, 9=A
  const int bassRoots[4] = {9, 5, 7, 0};
  // Speaker-calibrated registers. jbass1 (bass) is NOT shifted by the
  // pipeline (percussion register kept, octave 2). Sustained voices
  // (synth2/guitar1/pad1...) now ship +12 semitones from make_samples, so
  // arp/pad octaves step down one: identical ear-pitch at HALF the read
  // ratio, which is what collapses the shrill upsampling aliasing.
  const int bassOcts[4] = {2, 2, 2, 2};
  const Step arp[4][4] = {
      {{0, 1}, {4, 1}, {9, 1}, {4, 1}},   // Am: C E A E
      {{5, 1}, {9, 1}, {0, 2}, {9, 1}},   // F:  F A c A
      {{7, 1}, {11, 1}, {2, 2}, {11, 1}}, // G:  G B d B
      {{0, 1}, {4, 1}, {7, 1}, {4, 1}},   // C:  C E G E
  };
  // Pad a full octave below the arp: two sustained harmonically-rich
  // voices in the same register beat against each other as buzz even on
  // good speakers; separation keeps the arp's octave leaps musical (ET)
  // while the pad reads as a bed.
  const Step padRoot[4] = {{9, 0}, {5, 0}, {7, 0}, {0, 0}};

  for (int pat = 0; pat < 4; pat++) {
    const bool drumsOn = (pat == 0 || pat == 1);
    const bool bassOn = (pat == 0 || pat == 2);
    const bool leadOn = (pat == 0 || pat == 3);
    const bool padOn = (pat == 0 || pat == 3);

    for (int step = 0; step < patternLength; step++) {
      const int cell = pat * patternLength + step;
      const int seg = step / 8;

      // Drums: kick on the downbeats, snare backbeat, hats on the off
      // eighths, open hat and clap turns at the ends of bars.
      if (drumsOn) {
        if (step % 8 == 4) {
          WriteDemoStep(0, cell, kSnare, 0, kInstDrums);
        } else if (step % 4 == 0) {
          WriteDemoStep(0, cell, kKick, 0, kInstDrums);
        } else if (step % 4 == 2) {
          WriteDemoStep(0, cell, kHatClosed, 0, kInstDrums);
        }
        if (step == 14) {
          WriteDemoStep(0, cell, kHatOpen, 0, kInstDrums);
        }
        if (step == 26) {
          WriteDemoStep(0, cell, kClap, 0, kInstDrums);
        }
      }

      // Bass: steady sixteenth drive on the chord root.
      if (bassOn) {
        WriteDemoStep(3, cell, bassRoots[seg], bassOcts[seg], kInstBass);
      }

      // Lead: sixteenth arpeggio an octave up, echoed by the delay below.
      if (leadOn) {
        WriteDemoStep(2, cell, arp[seg][step % 4].note, arp[seg][step % 4].oct,
                      kInstLead);
      }

      // Pad: chord root on each change, long envelope lets it ring.
      if (padOn && step % 8 == 0) {
        WriteDemoStep(1, cell, padRoot[seg].note, padRoot[seg].oct, kInstPad);
      }
    }
  }

  // Per-voice mix and effects (applied once; cells carry instruments/octaves).
  // All readers now normalize their sources to the voice knee (Voice.cpp
  // kSourceGain + measured per-instrument gains), so volume 1 is linear and
  // nothing saturates the ceiling; loudness comes from master staging.
  voices[0].SetVolume(1); // drums
  voices[1].SetVolume(1); // bass linear at source level
  voices[2].SetVolume(1); // lead linear
  voices[2].SetDelay(2);  // Tracker step-delay: two-step cell echo on lead
  voices[2].SetEnvelopeLength(90000);  // ~400 ms per arp note
  voices[3].SetVolume(1);              // pad linear
  voices[3].SetEnvelopeLength(240000); // ~1 s pad swell
}

void Tracker::SetCommand(Command command, int val) {
  switch (command) {
  case Command::SelectTrack:
    recording = false; // implicit commit before another track builds
    SetTrackNum(val);
    break;
  case Command::Tempo:
    SetBPM(val);
    break;
  case Command::Note:
    SetNote(val, selectedTrack);
    if (isPlaying) {
      recording = true; // a placed note makes the session open again
    }
    break;
  case Command::Octave:
    SetOctave(val);
    break;
  case Command::NoteLength:
    SetEnvelopeLength(val);
    break;
  case Command::Envelope:
    SetEnvelopeNum(val);
    break;
  case Command::Volume:
    SetVolume(val);
    break;
  case Command::Delay:
    SetDelay(val);
    break;
  case Command::Arp:
    // Keypad label "Arp" selects per-voice sample FX (EffectMode), not a
    // melodic arpeggio and not Tracker step-delay.
    SetArp(val);
    break;
  case Command::ClearTrack:
    ClearTrackNum(val);
    break;
  case Command::Pattern:
    SetPatternNum(val);
    break;
  case Command::ClearPattern:
    ClearPatternNum(val);
    break;
  case Command::NewSong:
    ClearAll(val);
    break;
  case Command::Play:
    if (recording) {
      recording = false; // commit the build; transport keeps playing
    } else {
      TogglePlayStop();
    }
    break;
  case Command::Instrument:
    currentVoice = instrumentBank == 0 ? val : 12 + val;
    break;
  case Command::SongMode:
    allPatternPlay = !allPatternPlay;
    break;
  case Command::BankToggle:
    instrumentBank = instrumentBank ? 0 : 1;
    break;
  case Command::PatternClipboard:
    if (val == 0)
      CopyPattern();
    if (val == 1) {
      PastePattern();
    }
    break;
  case Command::Trim:
    StepMasterTrim(val == 1); // F4+G = louder, F4+F# = quieter
    break;
  case Command::None:
    break; // main loop filters this out; kept for switch completeness
  }
}

void Tracker::SetArp(int val) {
  voices[selectedTrack].SetEffectMode(static_cast<EffectMode>(val));
};

void Tracker::SetBPM(int val) {
  bpm = bpms[val];
  bps = bpm / 60;
};

void Tracker::SetDelay(int val) {
  // Tracker step-delay: retrigger older grid cells (not EffectMode::Echo).
  if (val > 0)
    val += 1;
  voices[selectedTrack].delay = val;
};
void Tracker::SetEnvelopeNum(int val) {
  voices[selectedTrack].SetEnvelopeNum(val);
};

void Tracker::SetEnvelopeLength(int val) {
  voices[selectedTrack].SetEnvelopeLength((val + 1) * 30000);
};

void Tracker::SetOctave(int val) { voices[selectedTrack].SetOctave(val); };

void Tracker::SetVolume(int val) { voices[selectedTrack].SetVolume(val); };

void Tracker::SetNote(int val, int track) {
  if (isPlaying) {
    // Write at trackIndex; playback reads trackIndex - 1 (one-behind).
    // Recording stores the armed instrument bank selection (currentVoice).
    tracks[track][trackIndex] = static_cast<int8_t>(val + 1);
    trackOctaves[track][trackIndex] =
        static_cast<int8_t>(voices[selectedTrack].octave);
    trackInstruments[track][trackIndex] = static_cast<uint8_t>(currentVoice);
  } else {
    // Stopped audition: arm immediately (no grid write, no hybrid latch).
    voices[track].SetNote(val, false, -1, currentVoice);
  }
};

void Tracker::SetTrackNum(int val) { selectedTrack = val; };

void Tracker::ClearTrackNum(int val) {
  for (int step = patternLength * (currentPattern);
       step < patternLength * (currentPattern + 1); step++) {
    tracks[val][step] = 0;
  }
  voices[val].SetEffectMode(EffectMode::Off);
};

void Tracker::SetPatternNum(int val) {

  trackIndex = trackIndex - (patternLength * currentPattern);
  currentPattern = val;
  trackIndex += (patternLength * currentPattern);
};

void Tracker::ClearPatternNum(int val) {
  for (int track = 0; track < kTrackCount; track++) {
    for (int step = patternLength * (currentPattern);
         step < patternLength * (currentPattern + 1); step++) {
      tracks[track][step] = 0;
    }
    voices[track].SetEffectMode(EffectMode::Off);
  }
};

void Tracker::TogglePlayStop() {
  isPlaying = !isPlaying;
  if (!isPlaying) {
    recording = false; // stopping always commits the open session
  }
};

// TBD
void Tracker::CopyTrack() {};
void Tracker::PasteTrack() {};

void Tracker::CopyPattern() {
  for (int track = 0; track < kTrackCount; track++) {
    int copyIndex = 0;
    for (int step = patternLength * (currentPattern);
         step < patternLength * (currentPattern + 1); step++) {
      patternCopy[track][copyIndex] = tracks[track][step];
      patternCopyInstruments[track][copyIndex] = trackInstruments[track][step];
      patternCopyOctaves[track][copyIndex] = trackOctaves[track][step];
      copyIndex++;
    }
  }
};

void Tracker::PastePattern() {
  for (int track = 0; track < kTrackCount; track++) {
    int copyIndex = 0;
    for (int step = patternLength * (currentPattern);
         step < patternLength * (currentPattern + 1); step++) {
      tracks[track][step] = patternCopy[track][copyIndex];
      trackInstruments[track][step] = patternCopyInstruments[track][copyIndex];
      trackOctaves[track][step] = patternCopyOctaves[track][copyIndex];
      copyIndex++;
    }
  }
};

// TBD
void Tracker::SetPatternLength(int val) {};
void Tracker::SaveDefaultSong() {};

void Tracker::ClearAll(int val) {
  for (int track = 0; track < kTrackCount; track++) {
    for (int step = 0; step < kGridLength; step++) {
      tracks[track][step] = 0;
    }
    voices[track].SetDelay(0);
    voices[track].SetEffectMode(EffectMode::Off);
    voices[track].SetEnvelopeNum(0);
    voices[track].SetVolume(1);
    voices[track].SetOctave(0);
  }
  patternLength = 32 + (32 * val);
  pendingCount_ = 0;
};