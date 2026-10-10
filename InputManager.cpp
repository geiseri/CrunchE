#include "InputManager.h"

InputManager::InputManager() = default;

void InputManager::UpdateInput(char rawInput) {
  trackCommand = Command::None;
  trackCommandArgument = 0;
  ledCommand = LedCommand::None;

  if (!rawInput) {
    return;
  }
  // Keypad library emits 'A'..'L' for the twelve note keys (electrical
  // order, bottom row first) and 'M'..'P' for F1..F4.
  if (rawInput >= 'A' && rawInput <= 'L') {
    ProcessNoteKey(static_cast<NoteKey>(rawInput - 'A'));
  } else if (rawInput >= 'M' && rawInput <= 'P') {
    ProcessFunctionKey(static_cast<FunctionKey>(rawInput - 'M'));
  }
}

// ---------------------------------------------------------------------------
// State machine. Exactly two shapes of event:
//   Idle  + function key -> Armed        (LED command announces the arm)
//   Armed + any key      -> Apply        (command emitted, disarmed)
//   Idle  + note key     -> Note          (record/live-play)
// ---------------------------------------------------------------------------

void InputManager::ProcessNoteKey(NoteKey key) {
  if (!armed_) {
    trackCommand = Command::Note;
    trackCommandArgument = static_cast<int>(key);
    return;
  }
  const MappedCommand mapped = MapArmedNote(armedFunction_, key);
  if (mapped.command != Command::None) {
    trackCommand = mapped.command;
    trackCommandArgument = mapped.arg;
    ledCommand = LedCommand::Applied;  // release the arm display
    Disarm();
  }
}

void InputManager::ProcessFunctionKey(FunctionKey pressed) {
  if (armed_) {
    const MappedCommand mapped = MapArmedFunction(armedFunction_, pressed);
    if (mapped.command != Command::None) {
      trackCommand = mapped.command;
      trackCommandArgument = mapped.arg;
      ledCommand = LedCommand::Applied;
      Disarm();
    }
    return;
  }
  // Arm; the Arm* LED command tells LedManager which function key is lit.
  armed_ = true;
  armedFunction_ = pressed;
  ledCommand = ArmLedCommand(pressed);
}

void InputManager::Disarm() { armed_ = false; }

// ---------------------------------------------------------------------------
// Pure command tables (audited 1:1 against docs/InstA.png).
// ---------------------------------------------------------------------------

MappedCommand InputManager::MapArmedNote(FunctionKey armed, NoteKey key) {
  switch (armed) {
    case kFuncVoice:   return MapVoiceNote(key);
    case kFuncTone:    return MapToneNote(key);
    case kFuncPattern: return MapPatternNote(key);
    case kFuncSong:    return MapSongNote(key);
    default:           return {};
  }
}

MappedCommand InputManager::MapArmedFunction(FunctionKey armed,
                                             FunctionKey pressed) {
  switch (armed) {
    case kFuncVoice:   return MapVoiceFunction(pressed);
    case kFuncTone:    return MapToneFunction(pressed);
    case kFuncPattern: return MapPatternFunction(pressed);
    case kFuncSong:    return MapSongFunction(pressed);
    default:           return {};
  }
}

// KEYPAD-DOC: consumed by tools/make_instruction_image.py. Every ROW line
// maps key-labels | Command | free text; the generator REQUIRES the set of
// Commands below to equal the set this function returns - add/remove/rename
// a case without updating the doc and the keymap build fails loudly.
//
// F1 armed: voice selection (grid of instruments drawn from Voice.cpp).
MappedCommand InputManager::MapVoiceNote(NoteKey key) {
  // Bank 0 indices 0..11; bank 1 folding happens in Tracker 'I'.
  // HDR: Voice & pitch
  // ROW: C – B | Instrument | select instrument — F1 + note, bank set by F4 + D:
  switch (key) {
    case kKeyC:  return {Command::Instrument, 0};
    case kKeyCs: return {Command::Instrument, 1};
    case kKeyD:  return {Command::Instrument, 2};
    case kKeyDs: return {Command::Instrument, 3};
    case kKeyE:  return {Command::Instrument, 4};
    case kKeyF:  return {Command::Instrument, 5};
    case kKeyFs: return {Command::Instrument, 6};
    case kKeyG:  return {Command::Instrument, 7};
    case kKeyGs: return {Command::Instrument, 8};
    case kKeyA:  return {Command::Instrument, 9};
    case kKeyAs: return {Command::Instrument, 10};
    case kKeyB:  return {Command::Instrument, 11};
    default:     return {};
  }
}

// F2 armed: tone controls. Note-rows are ordered silkscreen top-to-bottom
// (G# row, E row, C row); the F1-F4 fn row is documented on MapToneFunction.
MappedCommand InputManager::MapToneNote(NoteKey key) {
  // HDR: Selected track — tone controls
  switch (key) {
    // ROW: G# – B | Envelope | envelope: 0 decay, 1 swell, 2 sustain, 3 loop
    // ROW: E – G | Delay | delay: 0 off, 1–3 = 2–4-step echo
    // ROW: C – D# | sample FX (Command::Arp -> EffectMode): 0 dry, 1/2 lowpass, 3 echo
    case kKeyC:  return {Command::Arp, 0};        // C..D#  : EffectMode
    case kKeyCs: return {Command::Arp, 1};
    case kKeyD:  return {Command::Arp, 2};
    case kKeyDs: return {Command::Arp, 3};
    case kKeyE:  return {Command::Delay, 0};      // E..G   : echo/delay
    case kKeyF:  return {Command::Delay, 1};
    case kKeyFs: return {Command::Delay, 2};
    case kKeyG:  return {Command::Delay, 3};
    case kKeyGs: return {Command::Envelope, 0};   // G#..B  : envelope shape
    case kKeyA:  return {Command::Envelope, 1};
    case kKeyAs: return {Command::Envelope, 2};
    case kKeyB:  return {Command::Envelope, 3};
    default:     return {};
  }
}

// F3 armed: patterns, tracks, clears.
MappedCommand InputManager::MapPatternNote(NoteKey key) {
  // HDR: Patterns, tracks & erase
  switch (key) {
    // ROW: G# – B | ClearTrack | clear track 1–4 (by key) — current pattern
    // ROW: E – G | Pattern | switch to pattern 1–4
    // ROW: C – D# | ClearPattern | clear current pattern — all 4 tracks
    case kKeyC:  return {Command::ClearPattern, 0};  // C..D# : clear pattern
    case kKeyCs: return {Command::ClearPattern, 1};
    case kKeyD:  return {Command::ClearPattern, 2};
    case kKeyDs: return {Command::ClearPattern, 3};
    case kKeyE:  return {Command::Pattern, 0};       // E..G  : switch pattern
    case kKeyF:  return {Command::Pattern, 1};
    case kKeyFs: return {Command::Pattern, 2};
    case kKeyG:  return {Command::Pattern, 3};
    case kKeyGs: return {Command::ClearTrack, 0};    // G#..B : clear track
    case kKeyA:  return {Command::ClearTrack, 1};
    case kKeyAs: return {Command::ClearTrack, 2};
    case kKeyB:  return {Command::ClearTrack, 3};
    default:     return {};
  }
}

// F4 armed: song, transport, tempo, master trim.
MappedCommand InputManager::MapSongNote(NoteKey key) {
  // HDR: Song, transport & tempo
  switch (key) {
    // ROW: G# – B | Tempo | tempo: 120 / 132 / 145 / 180 BPM
    // ROW: E / F | PatternClipboard | copy / paste current pattern (all 4 tracks)
    // ROW: F# / G | Trim | master level: step down / step up (7-position trim)
    // ROW: C / C# | NewSong | new song: 32 / 64 steps — clears all, resets voices
    // ROW: D | BankToggle | toggle instrument bank 0 / 1
    // ROW: D# | SongMode | song mode on/off: auto-cycle all patterns
    case kKeyC:  return {Command::NewSong, 0};           // 32-step song
    case kKeyCs: return {Command::NewSong, 1};           // 64-step song
    case kKeyD:  return {Command::BankToggle, 0};        // instrument bank
    case kKeyDs: return {Command::SongMode, 0};          // play-all-patterns
    case kKeyE:  return {Command::PatternClipboard, 0};  // copy pattern
    case kKeyF:  return {Command::PatternClipboard, 1};  // paste pattern
    case kKeyFs: return {Command::Trim, 0};              // master level down
    case kKeyG:  return {Command::Trim, 1};              // master level up
    case kKeyGs: return {Command::Tempo, 0};             // 120 BPM
    case kKeyA:  return {Command::Tempo, 1};             // 132 BPM
    case kKeyAs: return {Command::Tempo, 2};             // 145 BPM
    case kKeyB:  return {Command::Tempo, 3};             // 180 BPM
    default:     return {};
  }
}

// ---- function + function combos ------------------------------------------

MappedCommand InputManager::MapVoiceFunction(FunctionKey pressed) {
  // ROW: F1 – F4 | Octave | octave 0 / 1 / 2 / 3   (0 = sample's pitch, ×2 each step)
  switch (pressed) {
    case kFuncVoice:   return {Command::Octave, 0};
    case kFuncTone:    return {Command::Octave, 1};
    case kFuncPattern: return {Command::Octave, 2};
    case kFuncSong:    return {Command::Octave, 3};
    default:           return {};
  }
}

MappedCommand InputManager::MapToneFunction(FunctionKey pressed) {
  // ROW: F1 – F4 | Volume | volume: 0 mute, 1 normal, 2 double, 3 overdrive
  switch (pressed) {
    case kFuncVoice:   return {Command::Volume, 0};
    case kFuncTone:    return {Command::Volume, 1};
    case kFuncPattern: return {Command::Volume, 2};
    case kFuncSong:    return {Command::Volume, 3};
    default:           return {};
  }
}

MappedCommand InputManager::MapPatternFunction(FunctionKey pressed) {
  // ROW: F1 – F4 | SelectTrack | select track 1–4
  switch (pressed) {
    case kFuncVoice:   return {Command::SelectTrack, 0};
    case kFuncTone:    return {Command::SelectTrack, 1};
    case kFuncPattern: return {Command::SelectTrack, 2};
    case kFuncSong:    return {Command::SelectTrack, 3};
    default:           return {};
  }
}

MappedCommand InputManager::MapSongFunction(FunctionKey pressed) {
  // ROW: F1 – F3 | NoteLength | note length: short / medium / long
  // ROW: F4 | Play | commit build if recording, else play / stop
  switch (pressed) {
    case kFuncVoice:   return {Command::NoteLength, 0};  // short
    case kFuncTone:    return {Command::NoteLength, 1};  // medium
    case kFuncPattern: return {Command::NoteLength, 2};  // long
    case kFuncSong:    return {Command::Play, 0};        // F4 + F4
    default:           return {};
  }
}

void InputManager::EndFrame() {
  ledCommand = LedCommand::None;
  trackCommand = Command::None;
  trackCommandArgument = 0;
}
