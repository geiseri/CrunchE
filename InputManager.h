#ifndef InputManager_h
#define InputManager_h

#include <cstdint>

// ---------------------------------------------------------------------------
// Keypad semantics - the single place note/function keys and their commands
// are defined. Values keep the historical command characters so serial
// traces, docs/InstA.png and muscle memory map 1:1.
// Audit target: every entry here is exercised by the native gate scripts.
// ---------------------------------------------------------------------------

// Note keys in electrical order (bottom silkscreen row first):
// 0 = C ... 11 = B. No count sentinels - they leak into every switch.
enum NoteKey : int8_t {
  kKeyC = 0,
  kKeyCs,
  kKeyD,
  kKeyDs,
  kKeyE,
  kKeyF,
  kKeyFs,
  kKeyG,
  kKeyGs,
  kKeyA,
  kKeyAs,
  kKeyB,
};

// Function keys F1..F4. Each arms a distinct command family.
enum FunctionKey : int8_t {
  kFuncVoice = 0,   // F1: instruments + octaves
  kFuncTone,        // F2: arp/filter, delay, envelope, volume
  kFuncPattern,     // F3: tracks, pattern select, clears
  kFuncSong,        // F4: song, transport, tempo, master trim
};

// Commands emitted to Tracker::SetCommand. None is the per-frame idle value.
enum class Command : char {
  None = ' ',
  Note = 'N',                  // bare C-B: record while playing / play when stopped
  Instrument = 'I',            // F1 + note
  Octave = 'O',                // F1 + F1..F4
  Arp = 'A',                   // F2 + C..D#
  Delay = 'D',                 // F2 + E..G
  Envelope = 'E',              // F2 + G#..B
  Volume = 'V',                // F2 + F1..F4
  SelectTrack = 'T',           // F3 + F1..F4
  ClearPattern = '#',          // F3 + C..D#
  Pattern = '$',               // F3 + E..G
  ClearTrack = '^',            // F3 + G#..B
  NewSong = 'X',               // F4 + C/C# (32/64 steps)
  BankToggle = 'H',            // F4 + D
  SongMode = 'C',              // F4 + D#
  PatternClipboard = '*',      // F4 + E/F: val 0 copy, 1 paste
  Trim = 'M',                  // F4 + F#/G: val 0 down, 1 up
  Tempo = 'B',                 // F4 + G#..B: BPM index 0..3
  NoteLength = 'L',            // F4 + F1..F3
  Play = 'P',                  // F4 + F4
};

// Commands emitted to LedManager::SetCommand for the four-LED function
// strip. Arm* holds the corresponding LED lit (and freezes blink displays)
// while a function is armed; Applied releases the hold - it is the strip's
// neutral state, so LedManager also boots there.
enum class LedCommand : char {
  None = ' ',        // no LED event this frame
  ArmVoice = 'A',    // F1 armed
  ArmTone = 'B',     // F2 armed
  ArmPattern = 'C',  // F3 armed
  ArmSong = 'D',     // F4 armed
  Applied = 'T',     // combo consumed: clear arm display, resume blinks
};

inline LedCommand ArmLedCommand(FunctionKey armed) {
  switch (armed) {
    case kFuncVoice:   return LedCommand::ArmVoice;
    case kFuncTone:    return LedCommand::ArmTone;
    case kFuncPattern: return LedCommand::ArmPattern;
    case kFuncSong:    return LedCommand::ArmSong;
  }
  return LedCommand::None;  // non-zero garbage cannot index; belt for UB
}

// Result of the pure mapping tables: what to emit and with which argument.
struct MappedCommand {
  Command command = Command::None;
  int arg = 0;
};

class InputManager {
 public:
  // Per-frame output window consumed by the main loop.
  Command trackCommand = Command::None;
  int trackCommandArgument = 0;
  LedCommand ledCommand = LedCommand::None;

  InputManager();

  void UpdateInput(char rawInput);
  void EndFrame();

  // Input is a two-phase machine, fully resolved within one UpdateInput
  // call: Idle --function key--> Armed(func) --note/function key-->
  // Apply (emit command, clear armed) --> Idle. phase()/armedFunction()
  // expose that state for tracing.
  enum class Phase : uint8_t { Idle, Armed };
  Phase phase() const { return armed_ ? Phase::Armed : Phase::Idle; }
  FunctionKey armedFunction() const { return armedFunction_; }

 private:
  void ProcessNoteKey(NoteKey key);
  void ProcessFunctionKey(FunctionKey pressed);

  // Pure mappings (armed state + key -> command): side-effect free, so the
  // whole keypad contract fits in switches and can be audited against the
  // instruction sheet without reading the state machine. One mapper per
  // armed function; every case label is an explicit key enum value.
  static MappedCommand MapArmedNote(FunctionKey armed, NoteKey key);
  static MappedCommand MapArmedFunction(FunctionKey armed, FunctionKey pressed);

  static MappedCommand MapVoiceNote(NoteKey key);      // F1 + C..B
  static MappedCommand MapToneNote(NoteKey key);       // F2 + C..B
  static MappedCommand MapPatternNote(NoteKey key);    // F3 + C..B
  static MappedCommand MapSongNote(NoteKey key);       // F4 + C..B

  static MappedCommand MapVoiceFunction(FunctionKey pressed);     // F1 + F1..F4
  static MappedCommand MapToneFunction(FunctionKey pressed);      // F2 + F1..F4
  static MappedCommand MapPatternFunction(FunctionKey pressed);   // F3 + F1..F4
  static MappedCommand MapSongFunction(FunctionKey pressed);      // F4 + F1..F4

  void Disarm();

  bool armed_ = false;
  FunctionKey armedFunction_ = kFuncVoice;
};

#endif
