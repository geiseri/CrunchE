// Native gate: multi-note recording flows through InputManager + Tracker.
//
// Covers what the single-shot command matrix cannot: phrase building over
// consecutive steps, same-step overwrite (last-write-wins auditioning),
// instrument/octave captured per cell at placement time, commit semantics
// for sessions, and the guarantee that stopped-mode keys NEVER write the
// grid. Uses the read-only CellAt/Octave/Instrument audit hooks.
#include "InputManager.h"
#include "Tracker.h"

#include <cstdio>

namespace {

int failures = 0;

void check(bool ok, const char *what) {
  if (!ok) {
    std::printf("  FAIL: %s\n", what);
    ++failures;
  }
}

// Simulated clock: 125 ms advances exactly one 16th step at 120 BPM
// (2 beats/s -> delta*2 reaches the 250-unit step quantum).
uint32_t g_now = 0;

}  // namespace

namespace tracker_clock {
uint32_t Now() { return g_now; }
}  // namespace tracker_clock

namespace {

void advanceSteps(Tracker &tr, int steps) {
  for (int step = 0; step < steps; step++) {
    g_now += 125;
    (void)tr.UpdateTracker();
  }
}

void key(InputManager &im, Tracker &tr, char keyChar) {
  im.UpdateInput(keyChar);
  if (im.trackCommand != Command::None) {
    tr.SetCommand(im.trackCommand, im.trackCommandArgument);
  }
  im.EndFrame();
}

void combo(InputManager &im, Tracker &tr, char functionChar, char keyChar) {
  key(im, tr, functionChar);
  key(im, tr, keyChar);
}

void TestPhraseBuild() {
  std::printf("== Multi-note phrase build ==\n");
  InputManager im;
  Tracker tr;  // constructor boots playing, empty grid

  key(im, tr, 'A');  // note C at step 0 (cell stores note+1)
  check(tr.CellAt(0, 0) == 1, "first note lands on step 0");
  check(tr.recording, "placing a note opens the session");

  advanceSteps(tr, 1);
  key(im, tr, 'C');  // note D at step 1
  check(tr.CellAt(0, 1) == 3, "second note lands on step 1");

  key(im, tr, 'D');  // note E, SAME step: replace, not scatter
  check(tr.CellAt(0, 1) == 4, "same-step re-entry overwrites (last wins)");
  check(tr.CellAt(0, 0) == 1, "earlier steps untouched by overwrite");

  advanceSteps(tr, 1);                        // step 2
  advanceSteps(tr, 1);                        // step 3
  key(im, tr, 'E');                           // note F at step 3 (char 'E'->4)
  check(tr.CellAt(0, 2) == 0, "skipped step stays a rest");
  check(tr.CellAt(0, 3) == 5, "note lands after gap");

  combo(im, tr, 'P', 'P');                    // F4+F4 -> commit
  check(!tr.recording && tr.isPlaying, "commit keeps transport running");
  check(tr.CellAt(0, 0) == 1 && tr.CellAt(0, 1) == 4 && tr.CellAt(0, 3) == 5,
        "phrase survives commit");

  key(im, tr, 'A');  // post-commit entry: places again, reopens session
  check(tr.recording, "post-commit note reopens session");
}

void TestCellAttributes() {
  std::printf("== Instrument + octave captured at placement ==\n");
  InputManager im;
  Tracker tr;

  combo(im, tr, 'M', 'E');  // F1 + E -> Instrument 4 (pad1)
  key(im, tr, 'A');         // place
  check(tr.CellInstrument(0, 0) == 4, "cell stores the armed instrument");

  combo(im, tr, 'M', 'O');  // F1 + F3 -> Octave 2 (voice selectedTrack)
  advanceSteps(tr, 1);
  key(im, tr, 'G');         // place at step 1 (char 'G' = F#, index 6)
  check(tr.CellOctave(0, 1) == 2, "cell stores the voice octave");
  check(tr.CellAt(0, 1) == 7, "note value placed (F#=6 -> cell 7)");

  // Changing instrument AFTER placing must not rewrite earlier cells.
  combo(im, tr, 'M', 'A');  // F1 + C -> drums
  check(tr.CellInstrument(0, 1) == 4, "past cells keep their instrument");
}

void TestStoppedAuditionNeverWrites() {
  std::printf("== Stopped transport: keys audition, never record ==\n");
  InputManager im;
  Tracker tr;

  combo(im, tr, 'P', 'P');  // session not open -> stop transport
  check(!tr.isPlaying, "stopped");

  for (char keyChar = 'A'; keyChar <= 'L'; keyChar++) {
    key(im, tr, keyChar);  // hammer every note key
  }
  bool anyCell = false;
  for (int step = 0; step < 32; step++) {
    anyCell = anyCell || tr.CellAt(0, step) != 0;
  }
  check(!anyCell, "no writes to the grid while stopped");
  check(!tr.recording, "stopped keys never open a session");

  combo(im, tr, 'P', 'P');  // start again
  check(tr.isPlaying, "restart via F4+F4");
  key(im, tr, 'A');
  check(tr.CellAt(0, 0) == 1, "recording works again after restart");
}

void TestTrackSwitchIsolation() {
  std::printf("== Mid-session track switch commits and isolates ==\n");
  InputManager im;
  Tracker tr;

  key(im, tr, 'A');                 // build on track 0, session open
  combo(im, tr, 'O', 'N');          // F3+F2 -> SelectTrack 1 (implicit commit)
  check(!tr.recording, "track switch implicitly commits");
  check(tr.selectedTrack == 1, "track 2 selected");

  advanceSteps(tr, 1);
  key(im, tr, 'B');                 // now building track 2
  check(tr.CellAt(1, 1) == 2, "note lands on the new track");
  check(tr.CellAt(0, 1) == 0, "previous track untouched by new builds");
}

void TestHybridLatchAppliesNextSample() {
  std::printf("== Hybrid latch: step edge then next sample ==\n");
  InputManager im;
  Tracker tr;

  key(im, tr, 'A');  // note at write cursor (step 0)
  check(tr.CellAt(0, 0) == 1, "hybrid fixture: note stored at step 0");

  g_now += 125;
  (void)tr.UpdateTracker();  // Phase A latches; SetNote deferred
  check(tr.lastTriggeredMask == 0,
        "step boundary latches without applying (mask clear)");

  // Same wall time: Phase B consumes the latch on the next sample frame.
  (void)tr.UpdateTracker();
  check((tr.lastTriggeredMask & 0x1) != 0,
        "next UpdateTracker sample applies pending trigger");
}

}  // namespace

int main() {
  TestPhraseBuild();
  TestCellAttributes();
  TestStoppedAuditionNeverWrites();
  TestTrackSwitchIsolation();
  TestHybridLatchAppliesNextSample();
  if (failures) {
    std::printf("RESULT: FAIL (%d)\n", failures);
    return 1;
  }
  std::printf("RESULT: PASS\n");
  return 0;
}
