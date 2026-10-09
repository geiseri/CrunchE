#include "LedManager.h"
#include "Arduino.h"

LedManager::LedManager(int pinA, int pinB, int pinC, int pinD) {

  outPinA = pinA;
  outPinB = pinB;
  outPinC = pinC;
  outPinD = pinD;
  pinMode(outPinA, OUTPUT);
  pinMode(outPinB, OUTPUT);
  pinMode(outPinC, OUTPUT);
  pinMode(outPinD, OUTPUT);
  lastMillis = millis();
  lastBlinkMillis = millis();
  command = LedCommand::Applied;  // neutral: blinks allowed from boot
}

void LedManager::SetPattern(bool pPlay, int p) {
  patternPlay = pPlay;
  pattern = p;
}

void LedManager::writePin(int i, int level) {
  switch (i) {
    case 0:  digitalWrite(outPinA, level); break;
    case 1:  digitalWrite(outPinB, level); break;
    case 2:  digitalWrite(outPinC, level); break;
    default: digitalWrite(outPinD, level); break;
  }
}

bool LedManager::isIdle() const {
  if (timeLit > 0) {
    return false;
  }
  for (int i = 0; i < 4; i++) {
    if (blink[i].active) {
      return false;
    }
  }
  return true;
}

void LedManager::UpdateLed() {
  const unsigned long now = millis();
  if (command == LedCommand::Applied) {
    float timeDelta = static_cast<float>(now) - lastMillis;
    lastMillis = static_cast<float>(now);

    if (timeLit > 0) {
      timeLit -= timeDelta;
      if (timeLit <= 0) {
        // Retract the metronome pulse, but never fight an active slow blink.
        for (int i = 0; i < 4; i++) {
          if (!blink[i].active) {
            writePin(i, LOW);
          }
        }
      }
    }

    // Slow-blink engine: 1 s on, 0.5 s off while a voice keeps triggering;
    // a full cycle without a trigger means its notes stopped replaying.
    for (int i = 0; i < 4; i++) {
      VoiceBlink &b = blink[i];
      if (!b.active) {
        continue;
      }
      if (now - b.lastTrig >= kBlinkIdleMs) {
        b.active = false;
        writePin(i, LOW);
        continue;
      }
      if (b.on) {
        if (now - b.cycleStart >= kBlinkOnMs) {
          b.on = false;
          b.cycleStart = now;
          writePin(i, LOW);
        }
      } else if (now - b.cycleStart >= kBlinkOffMs) {
        b.on = true;
        b.cycleStart = now;
        writePin(i, HIGH);
      }
    }
  }
  if (patternPlay) {
    // Start-of-display edge: clear any held activity pulse so the blink
    // begins from a dark strip, and restart the blink clock.
    if (!patternPlayWas) {
      patternPlayWas = true;
      lastBlinkMillis = millis();
      flipBlink = false;
      digitalWrite(outPinA, LOW);
      digitalWrite(outPinB, LOW);
      digitalWrite(outPinC, LOW);
      digitalWrite(outPinD, LOW);
    }
    // Toggle at ~4 Hz (writes only on transitions, not every audio frame).
    if (millis() - lastBlinkMillis >= 250) {
      lastBlinkMillis = millis();
      flipBlink = !flipBlink;
      const int level = flipBlink ? HIGH : LOW;
      switch (pattern) {
        case 0:
          digitalWrite(outPinA, level);
          break;
        case 1:
          digitalWrite(outPinB, level);
          break;
        case 2:
          digitalWrite(outPinC, level);
          break;
        case 3:
          digitalWrite(outPinD, level);
          break;
      }
    }
  } else {
    patternPlayWas = false;
  }
}

void LedManager::SetCommand(LedCommand com) {
  command = com;
  digitalWrite(outPinA, LOW);
  digitalWrite(outPinB, LOW);
  digitalWrite(outPinC, LOW);
  digitalWrite(outPinD, LOW);
  switch (command) {
    case LedCommand::ArmVoice:
      digitalWrite(outPinA, HIGH);
      break;
    case LedCommand::ArmTone:
      digitalWrite(outPinB, HIGH);
      break;
    case LedCommand::ArmPattern:
      digitalWrite(outPinC, HIGH);
      break;
    case LedCommand::ArmSong:
      digitalWrite(outPinD, HIGH);
      break;
    case LedCommand::Applied:  // neutral: all clear, blinks resume
    case LedCommand::None:     // no event this frame
      break;
  }
}

void LedManager::SetLit(float time, int col) {

  if (command != LedCommand::Applied || col < 0 || col > 3) {
    return;
  }
  // A track whose LED is mid slow-blink already shows its activity; the
  // metronome pulse must not re-light it out of phase.
  if (blink[col].active) {
    return;
  }

  writePin(0, LOW);
  writePin(1, LOW);
  writePin(2, LOW);
  writePin(3, LOW);
  writePin(col, HIGH);
  timeLit = time;
}

void LedManager::SetLitMask(uint8_t mask) {
  if (command != LedCommand::Applied) {
    return;
  }
  const unsigned long now = millis();
  for (int i = 0; i < 4; i++) {
    if (!(mask & (1u << i))) {
      continue;
    }
    VoiceBlink &b = blink[i];
    // First trigger (or a trigger after the cycle died out) starts a fresh
    // slow blink; later triggers just extend its life in the current phase.
    if (!b.active || now - b.lastTrig >= kBlinkIdleMs) {
      b.active = true;
      b.on = true;
      b.cycleStart = now;
      writePin(i, HIGH);
    }
    b.lastTrig = now;
  }
}