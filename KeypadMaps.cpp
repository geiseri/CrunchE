#include "InputManager.h"

// ---------------------------------------------------------------------------
// Armed-key → Command tables — SOURCE OF TRUTH for the keypad contract.
//
// KEYPAD-DOC comments are consumed by tools/keypad_contract.py to build
// docs/InstA.png and tests/native/keypad_matrix.generated.inc.
// Format:
//   // HDR: <section title>          (on the *Note* mapper for that F-key)
//   // ROW: <key labels> | <Command> | <help for the printout>
// The Command name in each ROW must match the Command:: returns in that
// function. Change a case without updating ROW (or the reverse) and
// `sh tools/gen_keypad.sh` fails.
// ---------------------------------------------------------------------------

// F1 armed: voice selection (instrument grids drawn from Voice.cpp).
MappedCommand InputManager::MapVoiceNote(NoteKey key) {
  // HDR: Voice & pitch
  // ROW: C - B | Instrument | select instrument - F1 + note, bank set by F4 + D
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

MappedCommand InputManager::MapVoiceFunction(FunctionKey pressed) {
  // ROW: F1 - F4 | Octave | octave 0 / 1 / 2 / 3   (0 = sample's pitch, x2 each step)
  switch (pressed) {
    case kFuncVoice:   return {Command::Octave, 0};
    case kFuncTone:    return {Command::Octave, 1};
    case kFuncPattern: return {Command::Octave, 2};
    case kFuncSong:    return {Command::Octave, 3};
    default:           return {};
  }
}

// F2 armed: tone controls.
MappedCommand InputManager::MapToneNote(NoteKey key) {
  // HDR: Selected track - tone controls
  switch (key) {
    // ROW: G# - B | Envelope | envelope: 0 decay, 1 swell, 2 sustain, 3 loop
    // ROW: E - G | Delay | delay: 0 off, 1-3 = 2-4-step echo
    // ROW: C - D# | Arp | sample FX (EffectMode): 0 dry, 1/2 lowpass, 3 echo
    case kKeyC:  return {Command::Arp, 0};
    case kKeyCs: return {Command::Arp, 1};
    case kKeyD:  return {Command::Arp, 2};
    case kKeyDs: return {Command::Arp, 3};
    case kKeyE:  return {Command::Delay, 0};
    case kKeyF:  return {Command::Delay, 1};
    case kKeyFs: return {Command::Delay, 2};
    case kKeyG:  return {Command::Delay, 3};
    case kKeyGs: return {Command::Envelope, 0};
    case kKeyA:  return {Command::Envelope, 1};
    case kKeyAs: return {Command::Envelope, 2};
    case kKeyB:  return {Command::Envelope, 3};
    default:     return {};
  }
}

MappedCommand InputManager::MapToneFunction(FunctionKey pressed) {
  // ROW: F1 - F4 | Volume | volume: 0 mute, 1 normal, 2 double, 3 overdrive
  switch (pressed) {
    case kFuncVoice:   return {Command::Volume, 0};
    case kFuncTone:    return {Command::Volume, 1};
    case kFuncPattern: return {Command::Volume, 2};
    case kFuncSong:    return {Command::Volume, 3};
    default:           return {};
  }
}

// F3 armed: patterns, tracks, clears.
MappedCommand InputManager::MapPatternNote(NoteKey key) {
  // HDR: Patterns, tracks & erase
  switch (key) {
    // ROW: G# - B | ClearTrack | clear track 1-4 (by key) - current pattern
    // ROW: E - G | Pattern | switch to pattern 1-4
    // ROW: C - D# | ClearPattern | clear current pattern - all 4 tracks
    case kKeyC:  return {Command::ClearPattern, 0};
    case kKeyCs: return {Command::ClearPattern, 1};
    case kKeyD:  return {Command::ClearPattern, 2};
    case kKeyDs: return {Command::ClearPattern, 3};
    case kKeyE:  return {Command::Pattern, 0};
    case kKeyF:  return {Command::Pattern, 1};
    case kKeyFs: return {Command::Pattern, 2};
    case kKeyG:  return {Command::Pattern, 3};
    case kKeyGs: return {Command::ClearTrack, 0};
    case kKeyA:  return {Command::ClearTrack, 1};
    case kKeyAs: return {Command::ClearTrack, 2};
    case kKeyB:  return {Command::ClearTrack, 3};
    default:     return {};
  }
}

MappedCommand InputManager::MapPatternFunction(FunctionKey pressed) {
  // ROW: F1 - F4 | SelectTrack | select track 1-4
  switch (pressed) {
    case kFuncVoice:   return {Command::SelectTrack, 0};
    case kFuncTone:    return {Command::SelectTrack, 1};
    case kFuncPattern: return {Command::SelectTrack, 2};
    case kFuncSong:    return {Command::SelectTrack, 3};
    default:           return {};
  }
}

// F4 armed: song, transport, tempo, master trim.
MappedCommand InputManager::MapSongNote(NoteKey key) {
  // HDR: Song, transport & tempo
  switch (key) {
    // ROW: G# - B | Tempo | tempo BPM from KeypadConstants.h kTempoBpmTable
    // ROW: E / F | PatternClipboard | copy / paste current pattern (all 4 tracks)
    // ROW: F# / G | Trim | master level: step down / step up (7-position trim)
    // ROW: C / C# | NewSong | new song: 32 / 64 steps - clears all, resets voices
    // ROW: D | BankToggle | toggle instrument bank 0 / 1
    // ROW: D# | SongMode | song mode on/off: auto-cycle all patterns
    case kKeyC:  return {Command::NewSong, 0};
    case kKeyCs: return {Command::NewSong, 1};
    case kKeyD:  return {Command::BankToggle, 0};
    case kKeyDs: return {Command::SongMode, 0};
    case kKeyE:  return {Command::PatternClipboard, 0};
    case kKeyF:  return {Command::PatternClipboard, 1};
    case kKeyFs: return {Command::Trim, 0};
    case kKeyG:  return {Command::Trim, 1};
    case kKeyGs: return {Command::Tempo, 0};
    case kKeyA:  return {Command::Tempo, 1};
    case kKeyAs: return {Command::Tempo, 2};
    case kKeyB:  return {Command::Tempo, 3};
    default:     return {};
  }
}

MappedCommand InputManager::MapSongFunction(FunctionKey pressed) {
  // ROW: F1 - F3 | NoteLength | note length: short / medium / long
  // ROW: F4 | Play | commit build if recording, else play / stop
  switch (pressed) {
    case kFuncVoice:   return {Command::NoteLength, 0};
    case kFuncTone:    return {Command::NoteLength, 1};
    case kFuncPattern: return {Command::NoteLength, 2};
    case kFuncSong:    return {Command::Play, 0};
    default:           return {};
  }
}
