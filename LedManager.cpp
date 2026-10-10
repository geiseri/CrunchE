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

void LedManager::SetPattern(bool patternPlayEnabled, int patternIndex) {
  patternPlay = patternPlayEnabled;
  pattern = patternIndex;
}

void LedManager::writePin(int ledIndex, int level) {
  switch (ledIndex) {
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
    default:
      break;
  }
}

void LedManager::resyncPins() {
  for (int ledIndex = 0; ledIndex < 4; ledIndex++) {
    const bool blinkOn = blink[ledIndex].active && blink[ledIndex].on;
    const bool metroOn =
        timeLit > 0 && ledIndex == litCol && !blink[ledIndex].active;
    writePin(ledIndex, (blinkOn || metroOn) ? HIGH : LOW);
  }
}

bool LedManager::isIdle() const {
  if (timeLit > 0) {
    return false;
  }
  for (int ledIndex = 0; ledIndex < 4; ledIndex++) {
    if (blink[ledIndex].active) {
      return false;
    }
  }
  return true;
}

void LedManager::UpdateLed() {
  const unsigned long now = millis();
  if (command != LedCommand::Applied) {
    // Freeze metronome decay while an arm hold owns the strip.
    lastMillis = now;
  } else {
    const float timeDelta = static_cast<float>(now - lastMillis);
    lastMillis = now;

    if (timeLit > 0) {
      timeLit -= timeDelta;
      if (timeLit <= 0) {
        timeLit = 0;
        litCol = -1;
        // Retract the metronome pulse, but never fight an active slow blink.
        for (int ledIndex = 0; ledIndex < 4; ledIndex++) {
          if (!blink[ledIndex].active) {
            writePin(ledIndex, LOW);
          }
        }
      }
    }

    // Slow-blink engine: 1 s on, 0.5 s off while a voice keeps triggering;
    // a full cycle without a trigger means its notes stopped replaying.
    for (int ledIndex = 0; ledIndex < 4; ledIndex++) {
      VoiceBlink &voiceBlink = blink[ledIndex];
      if (!voiceBlink.active) {
        continue;
      }
      if (now - voiceBlink.lastTrig >= kBlinkIdleMs) {
        voiceBlink.active = false;
        writePin(ledIndex, LOW);
        continue;
      }
      if (voiceBlink.on) {
        if (now - voiceBlink.cycleStart >= kBlinkOnMs) {
          voiceBlink.on = false;
          voiceBlink.cycleStart = now;
          writePin(ledIndex, LOW);
        }
      } else if (now - voiceBlink.cycleStart >= kBlinkOffMs) {
        voiceBlink.on = true;
        voiceBlink.cycleStart = now;
        writePin(ledIndex, HIGH);
      }
    }
  }  // command == Applied

  // Pattern blink only in Applied; arm holds must freeze all blink displays.
  if (patternPlay && command == LedCommand::Applied) {
    // Start-of-display edge: clear any held activity pulse so the blink
    // begins from a dark strip, and restart the blink clock.
    if (!patternPlayWas) {
      patternPlayWas = true;
      lastBlinkMillis = now;
      flipBlink = false;
      digitalWrite(outPinA, LOW);
      digitalWrite(outPinB, LOW);
      digitalWrite(outPinC, LOW);
      digitalWrite(outPinD, LOW);
    }
    // Toggle at ~4 Hz (writes only on transitions, not every audio frame).
    if (now - lastBlinkMillis >= 250) {
      lastBlinkMillis = now;
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
    // Drop the edge so arm→Applied or leaving song mode restarts cleanly.
    patternPlayWas = false;
  }
}

void LedManager::SetCommand(LedCommand com) {
  if (com == LedCommand::None) {
    return;
  }
  command = com;
  switch (command) {
    case LedCommand::ArmVoice:
      digitalWrite(outPinA, HIGH);
      digitalWrite(outPinB, LOW);
      digitalWrite(outPinC, LOW);
      digitalWrite(outPinD, LOW);
      break;
    case LedCommand::ArmTone:
      digitalWrite(outPinA, LOW);
      digitalWrite(outPinB, HIGH);
      digitalWrite(outPinC, LOW);
      digitalWrite(outPinD, LOW);
      break;
    case LedCommand::ArmPattern:
      digitalWrite(outPinA, LOW);
      digitalWrite(outPinB, LOW);
      digitalWrite(outPinC, HIGH);
      digitalWrite(outPinD, LOW);
      break;
    case LedCommand::ArmSong:
      digitalWrite(outPinA, LOW);
      digitalWrite(outPinB, LOW);
      digitalWrite(outPinC, LOW);
      digitalWrite(outPinD, HIGH);
      break;
    case LedCommand::Applied:
      // Resume blink/metronome phase on the pins (arm cleared them).
      resyncPins();
      break;
    case LedCommand::None:
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

  // Clear only idle pins so other tracks' slow blinks keep their phase.
  for (int ledIndex = 0; ledIndex < 4; ledIndex++) {
    if (!blink[ledIndex].active) {
      writePin(ledIndex, LOW);
    }
  }
  writePin(col, HIGH);
  timeLit = time;
  litCol = col;
}

void LedManager::SetLitMask(uint8_t mask) {
  if (command != LedCommand::Applied) {
    return;
  }
  const unsigned long now = millis();
  for (int ledIndex = 0; ledIndex < 4; ledIndex++) {
    if (!(mask & (1u << ledIndex))) {
      continue;
    }
    VoiceBlink &voiceBlink = blink[ledIndex];
    // First trigger (or a trigger after the cycle died out) starts a fresh
    // slow blink; later triggers just extend its life in the current phase.
    if (!voiceBlink.active || now - voiceBlink.lastTrig >= kBlinkIdleMs) {
      voiceBlink.active = true;
      voiceBlink.on = true;
      voiceBlink.cycleStart = now;
      writePin(ledIndex, HIGH);
    }
    voiceBlink.lastTrig = now;
  }
}
