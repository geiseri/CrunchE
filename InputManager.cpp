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
// Dispatch into KeypadMaps.cpp (source of truth for key → Command).
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

void InputManager::EndFrame() {
  ledCommand = LedCommand::None;
  trackCommand = Command::None;
  trackCommandArgument = 0;
}
