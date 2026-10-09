// Native gate: InputManager state machine + full command matrix.
//
// Verifies (a) every transition of Idle -> Armed -> Apply -> Idle, and
// (b) all 48 armed-note and 16 armed-function combinations map to the
// exact Command + argument published in docs/InstA.png. The expected
// table below is an INDEPENDENT restatement of the keypad contract -
// if InputManager.cpp and this table ever disagree, the sheet lies and
// this gate must fail.
#include "InputManager.h"

#include <cstdio>
#include <vector>

namespace {

int failures = 0;

const char *CommandName(Command c) {
  switch (c) {
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

Outcome press(InputManager &im, char key) {
  im.UpdateInput(key);
  const Outcome o{im.trackCommand, im.trackCommandArgument, im.ledCommand,
                  im.phase()};
  im.EndFrame();
  return o;
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
  const std::vector<Row> rows = {
      // F1 armed: every note selects an instrument; F-keys set octave.
      {'M', 'A', Command::Instrument, 0},  {'M', 'B', Command::Instrument, 1},
      {'M', 'C', Command::Instrument, 2},  {'M', 'D', Command::Instrument, 3},
      {'M', 'E', Command::Instrument, 4},  {'M', 'F', Command::Instrument, 5},
      {'M', 'G', Command::Instrument, 6},  {'M', 'H', Command::Instrument, 7},
      {'M', 'I', Command::Instrument, 8},  {'M', 'J', Command::Instrument, 9},
      {'M', 'K', Command::Instrument, 10}, {'M', 'L', Command::Instrument, 11},
      {'M', 'M', Command::Octave, 0},      {'M', 'N', Command::Octave, 1},
      {'M', 'O', Command::Octave, 2},      {'M', 'P', Command::Octave, 3},

      // F2 armed: arp/filter, delay, envelope by third-row register; F-keys
      // set the track volume.
      {'N', 'A', Command::Arp, 0},      {'N', 'B', Command::Arp, 1},
      {'N', 'C', Command::Arp, 2},      {'N', 'D', Command::Arp, 3},
      {'N', 'E', Command::Delay, 0},    {'N', 'F', Command::Delay, 1},
      {'N', 'G', Command::Delay, 2},    {'N', 'H', Command::Delay, 3},
      {'N', 'I', Command::Envelope, 0}, {'N', 'J', Command::Envelope, 1},
      {'N', 'K', Command::Envelope, 2}, {'N', 'L', Command::Envelope, 3},
      {'N', 'M', Command::Volume, 0},   {'N', 'N', Command::Volume, 1},
      {'N', 'O', Command::Volume, 2},   {'N', 'P', Command::Volume, 3},

      // F3 armed: clear pattern, switch pattern, clear track; F-keys select
      // the recording track.
      {'O', 'A', Command::ClearPattern, 0}, {'O', 'B', Command::ClearPattern, 1},
      {'O', 'C', Command::ClearPattern, 2}, {'O', 'D', Command::ClearPattern, 3},
      {'O', 'E', Command::Pattern, 0},      {'O', 'F', Command::Pattern, 1},
      {'O', 'G', Command::Pattern, 2},      {'O', 'H', Command::Pattern, 3},
      {'O', 'I', Command::ClearTrack, 0},   {'O', 'J', Command::ClearTrack, 1},
      {'O', 'K', Command::ClearTrack, 2},   {'O', 'L', Command::ClearTrack, 3},
      {'O', 'M', Command::SelectTrack, 0},  {'O', 'N', Command::SelectTrack, 1},
      {'O', 'O', Command::SelectTrack, 2},  {'O', 'P', Command::SelectTrack, 3},

      // F4 armed: song, transport, bank, clipboard, trim, tempo; F-keys set
      // note length or toggle play.
      {'P', 'A', Command::NewSong, 0},           {'P', 'B', Command::NewSong, 1},
      {'P', 'C', Command::BankToggle, 0},        {'P', 'D', Command::SongMode, 0},
      {'P', 'E', Command::PatternClipboard, 0},  {'P', 'F', Command::PatternClipboard, 1},
      {'P', 'G', Command::Trim, 0},              {'P', 'H', Command::Trim, 1},
      {'P', 'I', Command::Tempo, 0},             {'P', 'J', Command::Tempo, 1},
      {'P', 'K', Command::Tempo, 2},             {'P', 'L', Command::Tempo, 3},
      {'P', 'M', Command::NoteLength, 0},        {'P', 'N', Command::NoteLength, 1},
      {'P', 'O', Command::NoteLength, 2},        {'P', 'P', Command::Play, 0},
  };

  for (const Row &r : rows) {
    InputManager im;
    press(im, r.funcKey);  // arm
    const auto out = press(im, r.key);
    char label[96];
    std::snprintf(label, sizeof(label), "%c+%c -> %s %d", r.funcKey, r.key,
                  CommandName(r.want), r.arg);
    if (out.command != r.want || out.arg != r.arg) {
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
