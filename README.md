# The Arduino Sketch "OS"

Crunch-E is a keychain form factor music-making platform that is both limited and limitless. The current software supports 4 tracker-style tracks, 4 patterns, 21 instruments across two banks (plus dedicated drums and SFX voices), and live overdub recording with commit — but who knows where developers will take the open-source software next?

## DIY Build Parts

 ESP32 Dev Board
 Max98357 Breakout Board
 Arduino Compatible Keypad
 4 LEDs + Required Resistors

![alt text](https://raw.githubusercontent.com/xpndsprt/CrunchE/main/InstB.png)

## Pin configuration

All GPIO assignments are compile-time defines in `platformio.ini` (`build_flags`).
Change a value there and rebuild — `main.cpp` reads the macros, not hard-coded pins.

### Status strip LEDs (`LedManager` A–D)

| Define | Default GPIO | Role |
| --- | --- | --- |
| `PIN_LED_A` | 1 | Track / function LED A |
| `PIN_LED_B` | 2 | Track / function LED B |
| `PIN_LED_C` | 4 | Track / function LED C |
| `PIN_LED_D` | 8 | Track / function LED D |

### Keypad matrix

Rows are silkscreen `R4`→`R1` (top note row through bottom); columns are `L1`→`L4`.

| Define | Default GPIO | Silkscreen |
| --- | --- | --- |
| `PIN_KEYPAD_ROW0` | 13 | R4 |
| `PIN_KEYPAD_ROW1` | 3 | R3 |
| `PIN_KEYPAD_ROW2` | 44 | R2 |
| `PIN_KEYPAD_ROW3` | 43 | R1 |
| `PIN_KEYPAD_COL0` | 9 | L1 |
| `PIN_KEYPAD_COL1` | 10 | L2 |
| `PIN_KEYPAD_COL2` | 11 | L3 |
| `PIN_KEYPAD_COL3` | 12 | L4 |

### NeoPixel

| Define | Default GPIO | Role |
| --- | --- | --- |
| `PIN_NEOPIXEL` | 48 | RGB VU / status NeoPixel data |

### I2S → MAX98357A

| Define | Default GPIO | Amp pin |
| --- | --- | --- |
| `PIN_I2S_BCLK` | 6 | BCLK |
| `PIN_I2S_WS` | 5 | LRCLK (WS) |
| `PIN_I2S_DOUT` | 7 | DIN |

## CrunchOS Usage Instructions

Crunch-e is a sampler/tracker inspired by the Mod trackers of the 90s. It's easily built from inexpensive modules that an Arduino enthusiast might have on hand. Each track can play a single note of any instrument at a given time, resulting in 4-voice polyphony. The keypad handles a single press at a time, and any instructions below assume you press the function button first, release it, and then press another button.

### Function Buttons

Once the function button is pressed, it will light up the LED associated with that button. While the LED is lit (not blinking), you can make your function selection (like change tracks, volume, instrument, etc).

![CrunchE keypad reference — verified against current firmware](docs/InstA.png)

### Key Mapping

The keypad note rows are `G# A A# B`, `E F F# G`, and `C C# D D#`. In the firmware these map, from bottom row to top note row, to inputs `0-3`, `4-7`, and `8-11`. The top `F1-F4` row selects function inputs `0-3`.

### New Song

Press F4, then press C or C# to clear the tracks and set short or long patterns (32 or 64 steps per pattern).

### Play/Stop

Press F4 twice to stop or resume your song. Exception: if you have been
recording notes (a session is open), the first F4+F4 **commits** them and
playback keeps running — press it again to stop. See *Recording Notes*.

### BPM

Press F4, then press G#, A, A#, or B to set the tempo to 120, 132, 145, or 180 BPM, respectively.

### Instrument

Press F1, then press any of the note buttons to change the instrument within
the current bank. Press F4, then D to toggle the instrument bank: bank 0 is
drums (C), sound effects (C#), and the ten original instruments (D through
B); bank 1 adds nine melodic samples on C through G# (bass2, guitar1,
jlead3, jlead4, kick3, pad2, snareB3, synth1, synth3) with A, A#, and B as
silent placeholders.

### Octave

Press F1, then press F1, F2, F3, or F4 to set octave 0, 1, 2, or 3, respectively. The current keypad controls do not select octave -1.

### Track Volume / Overdrive

Press F2, then press any of the function buttons to change the volume. Press F2, then F4 to overdrive the sound, giving it a bit of distortion.

### Changing Tracks

Press F3, then press any of the function buttons to change tracks. You might want to record drums on track one, bass on track two, etc. Remember, only one voice per track can be played at a time, so for concurrent sounds, you need to record them on separate tracks. Switching tracks also commits any open recording session (see below), so notes you place next land only on the new track.

### Recording Notes

While the transport is running, a note key writes into the current 16th-note step of the selected track — you build a phrase live, alongside whatever the other tracks are already playing.

- Repeated presses inside one step replace that step's note (auditioning: last press wins). Presses that land on a later step add new notes.
- The instrument and octave active at the moment of the press are captured into the cell, so you can change voice mid-phrase.
- The first placed note opens a **record session**; the track LED then runs a slow blink (1 s on, 0.5 s off) while its notes replay.
- **Commit** with F4+F4: the session closes, playback and your phrase keep running. Placing another note opens a fresh session; commit again, then F4+F4 once more to stop the transport.
- While stopped, note keys audition on the selected voice and never touch the grid.

These flows are locked in by native gates: `tests/native/build_input_test.sh` (state machine + all 64 key combinations) and `tests/native/build_tracker_test.sh` (multi-note building, commit, per-cell attributes, stopped-mode safety).

### Strip LEDs

- A function key lit **solid** means it is armed — press the next key to apply, or the lit LED simply holds while you think.
- During playback each track's LED **slow-blinks** (1 s on / 0.5 s off) while its recorded notes are replaying — an LED blinking with no sound localizes a fault to the audio path (wiring/amp), a silent LED to the sequencer.
- Outside song mode, idle playback pulses the selected track on the beat (longer on the downbeat).
- In song mode with the strip otherwise dark, the current pattern's LED blinks at ~4 Hz: the device is waiting, not broken.

### Clearing Track

Press F3, then press G#, A, A#, or B to clear track 1, 2, 3, or 4, respectively.

### Instrument Length

Press F4, then F1, F2, or F3 to select one of three envelope-length settings. Pressing F4 twice while a recording session is open commits the notes you have been building (playback keeps running); pressing it twice with no open session toggles playback.

### Arpeggio/Delay/Envelope

Press F2, then choose a note key to set an effect for the selected track:

- C, C#, D, or D# selects arpeggio mode 0-3 (0 disables it).
- E, F, F#, or G selects delay setting 0-3 (0 disables it).
- G#, A, A#, or B selects envelope mode 0-3.

The printed low-pass effect labels are not implemented by the current firmware.

### Copy/Paste Patterns, Switch Patterns, and Play All Patterns in the Song

You can use up to four patterns. Switch to pattern 1, 2, 3, or 4 by pressing F3, then E, F, F#, or G, respectively. Clear the current pattern with F3, then C, C#, D, or D#; this clears all tracks in that pattern. Copy a pattern with F4, then E, and paste it with F4, then F. For example, copy a completed pattern, switch to an empty pattern, and paste it to create a variation. Press F4, then D# to toggle between playing the current pattern and playing all patterns sequentially. F4 followed by F# or G steps the master output level down or up (a 7-position trim centred on the factory default).

## Samples

The samples in `Samples/*.h` are **generated build artifacts — never edit them by hand**. The pipeline that produces them is deterministic and verified:

1. `Samples_src/*.wav` — pristine sources of truth: the upstream numbers extracted verbatim from the original headers **once** (raw signed 16-bit PCM at 22 050 Hz), frozen afterwards.
2. `tools/loops.py` derives everything from the pristine wavs on each run (idempotent, can never compound): optional octave register shift (`REGISTER_SHIFT`), optional saturation (`SATURATE`, off by default), attack skip, longest-first loop-seam search, micro-crossfade.
3. `gen_headers.py` writes `Samples/*.h` plus `SampleGains.h` — per-voice loudness gains normalized to the speaker's radiated band (tunables in `samplelib.py`).
4. Two byte-exact C++ gates prove both directions (`verify`: pristine wavs vs upstream headers; `verifygen`: generated headers vs their PCM).

Regenerate with one command: `sh tools/gen_all.sh`. To add or replace a sample, drop the WAV into `Samples_src/` and re-run. Samples live in program memory (the factory app partition has room for the full set).

Directory roles: `tools/` is the sample/artifact pipeline (generators, gates' C++ verifiers, the keymap image script - NOT tests), while `tests/native/` holds only the pass/fail test gates.

## Speaker Profiles (loudness-model tunables)

The gain table is generated against a model of **what the speaker can
actually radiate**: PCM energy outside that band either loads the master
limiter silently or is inaudible. When the transducer changes, update the
model - the gates then re-prove every header byte-exact.

### The knobs

| Knob | Where | Meaning |
| --- | --- | --- |
| `SPEAKER_BAND_HZ = (lo, hi)` | `tools/samplelib.py` | Radiated passband used as the loudness metric. **lo**: the speaker's usable low end — its ±3 dB spec floor, or ≈ its free-air resonance Fs if no spec (below Fs a driver falls ~12 dB/oct and makes no sound). **hi**: before cone break-up — ~6 kHz for micro drivers, 8 kHz mid-size, 10–12 kHz for full-range. |
| `BAND_ORDER` | `tools/samplelib.py` | Skirt steepness per edge (2 = 12 dB/oct Butterworth). |
| `TARGET_WRMS` | `tools/samplelib.py` | Per-voice in-band loudness target. A *relative* balance number, not a volume — changing it rescales all target-bound gains together. |
| `PEAK_CAP` | `tools/samplelib.py` | Raw int16 peak ceiling per voice (default 9000). Hardware headroom contract with the limiter knee (16000) / cap (30000); independent of speaker wattage. |
| `REGISTER_SHIFT`, `SATURATE`, `SAT_DRIVE` | `tools/loops.py` | Register shift (−12/0/+12; keep the demo octaves in `Tracker.cpp` paired) and tanh harmonic generation — small drivers need it, full-range speakers should run with `SATURATE = set()`. |
| `kMasterDiv`, `kMasterTrimTable` | `OutputMixer.h` | Actual boot loudness (÷4 default) and the live `F4+F#/G` trim steps — this is where speaker power handling and sensitivity get their due. |

### Reference profiles by resonance (Fs) and rating

| Driver class | Typical Fs | Rating | `SPEAKER_BAND_HZ` | Extra settings |
| --- | --- | --- | --- | --- |
| 10–14 mm keychain | 800–1000 Hz | 0.25–0.5 W | `(600, 6000)` | `SATURATE = {synths, pads, bass1…}`, `SAT_DRIVE 2–3`; demo voices +12 shifted (this was CrunchE's original profile) |
| 16–20 mm micro | 300–500 Hz | 0.5 W | `(400, 7000)` | `SATURATE` selectively; keep `REGISTER_SHIFT +12` or 0 by taste |
| 23–40 mm | 150–250 Hz | 0.5–1 W | `(250, 8000)` | `SATURATE = set()`; try `REGISTER_SHIFT 0` with octaves +1 |
| 50–57 mm full-range | 40–90 Hz | 1 W | `(150, 10000)` | `SATURATE = set()`; `REGISTER_SHIFT` 0–12; `kMasterDiv` may drop 4→2 if boot level feels shy (trim steps cover live taste) |

**0.5 W vs 1 W same band, different ceiling:** the wattage does *not* touch
`TARGET_WRMS`/`PEAK_CAP` (those are digital-domain contracts). It decides
whether the amp's rail-limited output (≈0.8 W @ 3.3 V, ≈1.8 W @ 5 V into
8 Ω) exceeds the driver's rating — if it does, `kMasterDiv` and the limiter
are the safety line; if it doesn't (1 W driver on 3.3 V), the amp itself is
the ceiling and you can afford `kMasterDiv = 2`.

### Deriving the band from a spec sheet (F0, response, THD curves)

A datasheet's resonance, frequency-response, and THD curves contain
everything `SPEAKER_BAND_HZ` needs. `python tools/band_from_curve.py` does
the arithmetic; the reasoning it encodes:

**Low edge `lo` — from the curve, fall back to F0.** An unbaffled driver is
acoustically a 2nd-order high-pass: below Fs cone excursion goes constant
([Linkwitz](https://www.linkwitzlab.com/models.htm) — *"below the driver
resonance Fs the excursion X1 becomes constant"*), so velocity ∝ f and SPL ∝
f² — a **12 dB/octave** rolloff. That is exactly why `BAND_ORDER = 2` is the
right low skirt, and why sub-F0 content doesn't just fade, it *vanishes*.
`lo` is the curve's −3 dB-from-midband corner (reference = mean response over
0.5–2 kHz). **When there's no curve: `lo ≈ F0(typ)`** — but the ignore
min/max columns assumes Qts ≈ 0.7: the −3 dB point equals Fs only at
Qts = 0.707 (maximally flat); a peaky Qts ≈ 1.5 puts it at ~0.7·Fs *with* a
+4 dB hump, and a damped Qts ≈ 0.3 doesn't reach −3 dB until ~3·Fs. So with
Qts (usually T/S parameters list it) or a curve, trust those; F0 alone is a
±3 dB-class estimate. F0 min/max (±10–20 %) matters less than that.

**High edge `hi` — curve vetoed by the THD curve.** The response curve keeps
looking alive into cone break-up where the sound is already filthy:
`hi = min(−3 dB corner, lowest f where THD(f) exceeds threshold)`. The
threshold must respect masking ([Audioholics](https://www.audioholics.com/loudspeaker-design/audibility-of-distortion-at-bass),
citing Fielder & Benjamin): at 110 dB a 20 Hz tone masks its 2nd harmonic up
to ~5 %, but a 5th harmonic only at fraction-of-1 % — low frequencies and
low harmonic orders are *more* tolerable, HF less so. In dense music even
30 % peaks went unnoticed. Practical rule for a toy amp at modest SPL:
**~5 % near/below the LF hump, ~3 % in the upper band, and re-check at your
actual drive level** (`kMasterDiv`-limited, often ≪ rated W — THD scales
hard with excursion).

**The LF THD hump also picks `SATURATE`.** Distortion peaks at/below Fs
(suspension/excursion nonlinearity — Klippel's standard `THD(Fs)` metric):
a driver showing a big THD hump near Fs is saying its fundamentals there are
dirty, exactly when `SATURATE` + a shift/harmonic generator (folding energy
*into* the band) is the honest fix. Cross-check: a voice whose energy sits
one octave below `lo` radiates ~1/16 the power (−12 dB, 2nd order) — that's
the "peak-capped but still quiet in-band" list in `loops.py`, from the datasheet's side.

**Tolerance, honestly:** edge placement errors are nearly free for voices
centered well above `lo` (weight ratio ≤ +2.6 dB for 20 % at the edge) but
reach ~+6 dB one octave *below* it — so voices straddling `lo` are the ones
that shift. `gen_all.sh` + the native gates + one listen beat any amount of
spreadsheet theorizing.

### Swapping a speaker — checklist

1. `tools/samplelib.py` — `SPEAKER_BAND_HZ` (+ `BAND_ORDER` if you want
   harder skirts), and `TARGET_WRMS`/`PEAK_CAP` only if the digital
   contract should change.
2. `tools/loops.py` — `REGISTER_SHIFT` (re-pair the demo octave registers
   in `Tracker.cpp` if you move it) and the `SATURATE` set.
3. `OutputMixer.h` — `kMasterDiv` boot trim / trim table span for the new
   power & sensitivity.
4. `README.md` — the wiring table speaker row and the power-budget note
   below.
5. Regenerate and prove: `sh tools/gen_all.sh`, run the native gates,
   flash.

(`docs/InstA.png` is keypad-only — unaffected by speaker choice. The
`SampleGains.h` banner's numbers auto-derive from the constants; it names
whatever `SPEAKER_BAND_HZ` currently says.)

**Have fun building, exploring, and making music!**

## Audio Amplifier — MAX98357A (Analog Devices / Maxim)

CrunchE's speaker is driven by a **MAX98357A**: a tiny, filterless Class-D
amplifier with an integrated I2S DAC — "Class AB audio performance with Class D
efficiency." It needs no MCLK line and no register programming; it auto-detects
the clocking scheme on BCLK/LRCLK/DIN. (The MAX98357**B** sibling is the same die
with left-justified instead of I2S support — this board uses the **A**.)

### Key specifications (from the datasheet)

| Parameter | Value |
| --- | --- |
| Supply range (VDD) | 2.5 V – 5.5 V |
| Output power | 3.2 W @ 4 Ω, 10 % THD+N (5 V supply) |
| | 1.8 W @ 8 Ω, 10 % THD+N (5 V supply) |
| | ~0.8 W @ 8 Ω (3.3 V supply) |
| THD+N | 0.013 % @ 1 kHz |
| Efficiency | 92 % (8 Ω, 1 W out) |
| Quiescent current | 2.4 mA |
| Output noise | 22.8 µV RMS (gain = 15 dB) |
| PSRR | 77 dB @ 1 kHz |
| Sample rates | 8 kHz – 96 kHz (16/24/32-bit I2S, PCM, or 8-ch TDM) |
| PWM switching | ~330 kHz — filtered by the speaker coil itself (no LC) |
| Gain (pin-strapped) | 3 / 6 / 9 / 12 / 15 dB — 9 dB with GAIN floating |
| Protection | short-circuit, thermal shutdown; click-and-pop suppression; GSM/TDMA RF immunity |
| Packages | 9-ball WLP 1.345 × 1.435 mm; 16-pin TQFN 3 × 3 mm (EP = ground for heat) |

### How CrunchE wires it

I2S pins are set in `platformio.ini` (`PIN_I2S_*`); see **Pin configuration** above.

| MAX98357A pin | Signal | Define | Default GPIO |
| --- | --- | --- | --- |
| BCLK (16) | bit clock | `PIN_I2S_BCLK` | 6 |
| LRCLK (14) | frame / "ws" | `PIN_I2S_WS` | 5 |
| DIN (1) | serial data | `PIN_I2S_DOUT` | 7 |
| SD_MODE (4) | shutdown + channel select | — | fixed on the sealed PCB (not firmware-controlled) |
| GAIN_SLOT (2) | gain select | — | fixed on the sealed PCB (not firmware-controlled) |
| OUTP/OUTN | BTL speaker pair | — | 8 Ω / 1 W, 55 mm full-range speaker |

The firmware runs `I2S_MODE_STD` (standard Philips I2S), 16-bit, mono slot at
22 050 Hz — well inside the chip's range. Outputs are bridge-tied (BTL): the
speaker sits between OUTP and OUTN with no ground reference, and the amp must
drive the speaker directly (it is not a preamp). SD_MODE below ~0.16 V shuts
the chip down; 0.16–0.77 V selects the (L+R)/2 mix, higher voltages select
right/left only.

⚠️ **Power budget note (depends on rail voltage and driver rating):** at 5 V
the amp can out-cook small drivers — ~1.8 W into 8 Ω exceeds both the keychain
0.5 W unit and a 1 W part, so the limiter/trim stages in firmware are what
keep the voice coil safe. At 3.3 V the amp reaches only ~0.8 W into 8 Ω:
fine and self-limiting for a 1 W driver (the amp, not the speaker, is the
ceiling — tune loudness against THD near max output), but still above the
0.5 W keychain unit's rating. Which limit binds: speaker dissipation
(5 V rail), or amp power (3.3 V rail with a ≥1 W driver).

Datasheet: <https://www.analog.com/media/en/technical-documentation/data-sheets/max98357a-max98357b.pdf>
