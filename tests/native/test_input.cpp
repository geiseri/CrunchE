// Native gate: InputManager state machine + full command matrix.
//
// Verifies (a) every transition of Idle -> Armed -> Apply -> Idle, and
// (b) all 64 armed combinations against the matrix derived from
// KeypadMaps.cpp (same SoT as docs/InstA.png). Regenerate with
// tools/gen_keypad.sh / tools/gen_keypad_matrix.py.
#include "InputManager.h"

#include <cstdio>
#include <vector>

namespace {

int failures = 0;

const char *CommandName(Command command) {
  switch (command) {
    case Command::None: return "None";
    case Command::Note: return "Note";
    case Command::Instrument: return "Instrument";
    case Command::Octave: return "Octave";
    case Command::Arp: return "Arp";
    case Command::Delay: return "Delay";
    case Command::Envelope: return "Envelope";
    case Command::Volume: return "Volume";
    case Command::SelectTrack: return "SelectTrack";
    case Command::ClearPattern: return "ClearPattern";
    case Command::Pattern: return "Pattern";
    case Command::ClearTrack: return "ClearTrack";
    case Command::NewSong: return "NewSong";
    case Command::BankToggle: return "BankToggle";
    case Command::SongMode: return "SongMode";
    case Command::PatternClipboard: return "PatternClipboard";
    case Command::Trim: return "Trim";
    case Command::Tempo: return "Tempo";
    case Command::NoteLength: return "NoteLength";
    case Command::Play: return "Play";
  }
  return "?";
}

void check(bool ok, const char *what) {
  if (!ok) {
    std::printf("  FAIL: %s\n", what);
    ++failures;
  }
}

// Drive one key through a fresh manager and capture its one-frame outputs.
struct Outcome {
  Command command;
  int arg;
  LedCommand led;
  InputManager::Phase phaseAfter;
};

Outcome press(InputManager &im, char keyChar) {
  im.UpdateInput(keyChar);
  const Outcome outcome{im.trackCommand, im.trackCommandArgument, im.ledCommand,
                        im.phase()};
  im.EndFrame();
  return outcome;
}

// ---- state machine --------------------------------------------------------

void TestTransitions() {
  std::printf("== Transitions ==\n");
  InputManager im;

  check(im.phase() == InputManager::Phase::Idle, "boots idle");

  const auto arm = press(im, 'M');  // F1
  check(arm.command == Command::None, "arming emits no track command");
  check(arm.led == LedCommand::ArmVoice, "arm emits ArmVoice LED command");
  check(arm.phaseAfter == InputManager::Phase::Armed, "F1 press arms");

  // Arming persists across idle frames (a user can take their time).
  press(im, '\0');
  press(im, '\0');
  check(im.phase() == InputManager::Phase::Armed, "armed survives idle frames");

  // Armed + note: apply, emit, disarm, announce on the strip.
  const auto apply = press(im, 'A');  // note C
  check(apply.command == Command::Instrument && apply.arg == 0,
        "F1+C applies Instrument 0");
  check(apply.led == LedCommand::Applied, "apply flashes the strip");
  check(apply.phaseAfter == InputManager::Phase::Idle, "apply disarms");

  // Armed + function: the second function key is consumed as a combo,
  // it must NOT re-arm or overwrite the armed function.
  press(im, 'O');                                     // arm F3
  const auto fcombo = press(im, 'M');                 // F3+F1
  check(fcombo.command == Command::SelectTrack && fcombo.arg == 0,
        "F3+F1 applies SelectTrack 0");
  check(fcombo.phaseAfter == InputManager::Phase::Idle, "combo disarms");

  // Same-key double press is a combo too (F4+F4 = Play, not re-arm).
  press(im, 'P');  // arm F4
  const auto play = press(im, 'P');
  check(play.command == Command::Play, "F4+F4 applies Play");
  check(play.phaseAfter == InputManager::Phase::Idle, "Play disarms");

  // Invalid keys: ignored, state untouched.
  const auto junk = press(im, 'Z');
  check(junk.command == Command::None && junk.led == LedCommand::None &&
            junk.phaseAfter == InputManager::Phase::Idle,
        "unknown key fully ignored");

  // Bare notes when idle record; argument is the electrical key index.
  const auto bare = press(im, 'F');  // key F = index 5
  check(bare.command == Command::Note && bare.arg == 5, "idle F emits Note 5");

  // Armed function identity is exposed for tracing.
  press(im, 'N');  // arm F2
  check(im.armedFunction() == kFuncTone, "armedFunction traces the arm");
}

// ---- command matrix -------------------------------------------------------

struct Row {
  char funcKey;   // 'M'..'P' pressed to arm
  char key;       // physical key char applied ('A'..'L' note, 'M'..'P' func)
  Command want;
  int arg;
};

void TestMatrix() {
  std::printf("== Command matrix ==\n");
#include "keypad_matrix.generated.inc"

  for (const Row &row : rows) {
    InputManager im;
    press(im, row.funcKey);  // arm
    const auto out = press(im, row.key);
    char label[96];
    std::snprintf(label, sizeof(label), "%c+%c -> %s %d", row.funcKey, row.key,
                  CommandName(row.want), row.arg);
    if (out.command != row.want || out.arg != row.arg) {
      std::printf("  FAIL: %s (got %s %d)\n", label, CommandName(out.command),
                  out.arg);
      ++failures;
    }
    if (out.phaseAfter != InputManager::Phase::Idle) {
      std::printf("  FAIL: %s did not disarm\n", label);
      ++failures;
    }
  }
  std::printf("  %zu combinations checked\n", rows.size());
}

}  // namespace

int main() {
  TestTransitions();
  TestMatrix();
  if (failures) {
    std::printf("RESULT: FAIL (%d)\n", failures);
    return 1;
  }
  std::printf("RESULT: PASS\n");
  return 0;
}
