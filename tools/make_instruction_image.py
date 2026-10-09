#!/usr/bin/env python3
"""Generate docs/InstA.png from the FIRMWARE, not from this file.

Section text is pulled from KEYPAD-DOC `ROW:`/`HDR:` comment blocks inside
the dispatch functions in InputManager.cpp, and the instrument grids are
pulled from the live `instrumentSources[]` table in Voice.cpp. The
generator HARD-FAILS if a mapper's returned Commands ever disagree with
its documented ROW set - so any key/function change must update the doc
comment or the keymap build breaks. That is the exposure mechanism.

    ~/.platformio/penv/bin/python tools/make_instruction_image.py
"""
import pathlib
import re
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parents[1]

# --------------------------------------------------------------------------
# Parsing the firmware contract
# --------------------------------------------------------------------------

NOTE_MAPPERS = {1: "MapVoiceNote", 2: "MapToneNote", 3: "MapPatternNote",
                4: "MapSongNote"}
FN_MAPPERS = {1: "MapVoiceFunction", 2: "MapToneFunction",
              3: "MapPatternFunction", 4: "MapSongFunction"}

ROW_RE = re.compile(r"//\s*ROW:\s*(.+?)\s*\|\s*(\w+)\s*\|\s*(.+)$")
HDR_RE = re.compile(r"//\s*HDR:\s*(.+)$")
RET_RE = re.compile(r"return\s*\{\s*Command::(\w+)")


def parse_mapper(src, name):
    """Return (rows[(label, cmd, desc)], hdr, returned_cmds) for one mapper."""
    sig = src.index(f"InputManager::{name}(")
    end = src.index("\n}", sig)
    body = src[sig:end]
    rows, hdr = [], None
    for line in body.splitlines():
        m = ROW_RE.search(line)
        if m:
            rows.append((m.group(1), m.group(2), m.group(3)))
        m = HDR_RE.search(line)
        if m:
            hdr = m.group(1).strip()
    returned = set(RET_RE.findall(body))
    documented = {r[1] for r in rows}
    if documented != returned:
        sys.exit(f"KEYPAD-DOC out of sync in {name}: "
                 f"documented {sorted(documented)} vs returns "
                 f"{sorted(returned)} - fix the ROW comments or the code")
    if rows and not returned:
        sys.exit(f"KEYPAD-DOC in {name}: rows documented but no Commands found")
    return rows, hdr, returned


def parse_instruments():
    """Voice.cpp instrumentSources[] -> ordered sample names (voiceNum 2+)."""
    voice = (ROOT / "Voice.cpp").read_text()
    table = voice[voice.index("instrumentSources[] = {"):]
    table = table[:table.index("};")]
    names = re.findall(r"\{(\w+),\s*kGain_\w+\}", table)
    if len(names) != 19:
        sys.exit(f"instrumentSources has {len(names)} entries, expected 19 - "
                 "update the grid logic/comment in this generator "
                 "deliberately, then re-run")
    tracker = (ROOT / "Tracker.cpp").read_text()
    if "12 + val" not in tracker:
        sys.exit("bank-fold rule (bank1 voiceNum = 12 + val) no longer found "
                 "in Tracker.cpp - update the grid derivation here")
    return names


def build_sections():
    src = (ROOT / "InputManager.cpp").read_text()
    sections = []
    for fnum in (1, 2, 3, 4):
        fn_rows, _, _ = parse_mapper(src, FN_MAPPERS[fnum])
        note_rows, hdr, _ = parse_mapper(src, NOTE_MAPPERS[fnum])
        if hdr is None:
            sys.exit(f"missing HDR: in {NOTE_MAPPERS[fnum]}")
        # Silkscreen order: F-key row first, then note rows as documented.
        rows = fn_rows + note_rows
        sections.append((str(fnum), hdr, rows))
    return sections


def build_grids(names):
    """Return per-bank cells [(key, label)] in silkscreen display order."""
    electrical = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    bank0 = {"C": "drums", "C#": "sfx"}            # voiceNum 0/1 special cases
    for i in range(10):                            # voiceNum 2..11
        bank0[electrical[2 + i]] = names[i]
    bank1 = {}
    for i in range(9):                             # voiceNum 12..20
        bank1[electrical[i]] = names[10 + i]
    for k in electrical[9:]:                       # voiceNum 21..23 unprovisioned
        bank1[k] = "silent"
    display_order = [8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3]  # G# row, E row, C row
    grids = []
    for bank in (bank0, bank1):
        cells = [(electrical[i], bank[electrical[i]]) for i in display_order]
        grids.append(cells)
    return grids


# --------------------------------------------------------------------------
# Rendering
# --------------------------------------------------------------------------

W, H = 1000, 1560
BG = (247, 246, 242)
INK = (28, 28, 32)
MUTED = (110, 108, 100)
ACCENT = (216, 96, 40)
KEY_BG = (254, 252, 247)
KEY_EDGE = (190, 186, 176)


def find_font(size, bold=False):
    """Resolve fonts by GENERIC family name - Pillow asks the OS
    (CoreText on macOS, fontconfig on Linux, registry on Windows), so no
    hardcoded paths. Order: bold-capable names, then any-portable
    standbys, then Pillow's own scalable default (bundled Aileron)."""
    names = (["Arial Bold", "Verdana Bold"] if bold else []) + [
        "Arial", "Verdana", "Helvetica", "DejaVu Sans", "Liberation Sans",
    ]
    for name in names:
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default(size=size)


F_TITLE = find_font(30, bold=True)
F_HEAD = find_font(21, bold=True)
F_KEY = find_font(23, bold=True)
F_LBL = find_font(18, bold=True)
F_NOTE = find_font(18)
F_SMALL = find_font(15)
F_BADGE = find_font(13, bold=True)

img = Image.new("RGB", (W, H), BG)
d = ImageDraw.Draw(img)


def badge(x, y, text, w=34, fill=ACCENT, fg=(255, 255, 255)):
    d.rounded_rectangle([x, y, x + w, y + 24], 6, fill=fill)
    bbox = d.textbbox((0, 0), text, font=F_BADGE)
    d.text((x + (w - (bbox[2] - bbox[0])) / 2 - bbox[0],
            y + (24 - (bbox[3] - bbox[1])) / 2 - bbox[1]), text, font=F_BADGE, fill=fg)


def text(x, y, s, font=F_NOTE, fill=INK):
    d.text((x, y), s, font=font, fill=fill)


def wrapped(x, y, s, font=F_SMALL, fill=MUTED, max_w=W - 80, line_h=18):
    words = s.split()
    line = []
    for w in words:
        trial = " ".join(line + [w])
        if line and d.textlength(trial, font=font) > max_w:
            text(x, y, " ".join(line), font=font, fill=fill)
            y += line_h
            line = [w]
        else:
            line = trial.split()
    if line:
        text(x, y, " ".join(line), font=font, fill=fill)
        y += line_h
    return y


def section(fnum, header, rows, y):
    badge(40, y + 1, "F" + fnum)
    text(84, y, header, font=F_HEAD)
    y += 34
    for label, _cmd, desc in rows:
        text(84, y, label, font=F_LBL, fill=ACCENT)
        text(214, y, desc, font=F_NOTE)
        y += 29
    return y + 12


def voice_grid(y, bank, cells):
    text(84, y, f"bank {bank}  (F1 + note):", font=F_NOTE, fill=ACCENT)
    y += 25
    for r in range(3):
        for c in range(4):
            note, name = cells[r * 4 + c]
            x = 84 + c * 224
            bbox = d.textbbox((0, 0), note, font=F_SMALL)
            text(x, y, note, font=F_SMALL, fill=INK)
            text(x + (bbox[2] - bbox[0]) + 8, y, name, font=F_SMALL, fill=MUTED)
        y += 22
    return y + 8


def main():
    sections = build_sections()
    grids = build_grids(parse_instruments())

    text(40, 30, "CrunchE Keypad Quick-ref", font=F_TITLE)


    ROWS = [
        ["F1", "F2", "F3", "F4"],
        ["G#", "A", "A#", "B"],
        ["E", "F", "F#", "G"],
        ["C", "C#", "D", "D#"],
    ]
    COLX, KW, KH, GAP = [170 + 202 * i for i in range(4)], 180, 60, 10
    TOP = 100
    for i, label in enumerate(["function row", "octave row", "octave row", "base notes"]):
        y = TOP + i * (KH + GAP)
        text(24, y + 24, label, font=F_SMALL, fill=MUTED)
        for j, key in enumerate(ROWS[i]):
            x = COLX[j]
            is_func = i == 0
            d.rounded_rectangle([x, y, x + KW, y + KH], 10,
                                fill=(ACCENT if is_func else KEY_BG),
                                outline=KEY_EDGE, width=2)
            bbox = d.textbbox((0, 0), key, font=F_KEY)
            d.text((x + (KW - bbox[2] + bbox[0]) / 2 - bbox[0],
                    y + (KH - bbox[3] + bbox[1]) / 2 - bbox[1]), key, font=F_KEY,
                   fill=(255, 255, 255) if is_func else INK)

    y = TOP + 4 * (KH + GAP) + 6
    text(COLX[0], y, "F1-F4 arm a function (its LED stays lit); the next key applies it.",
         font=F_SMALL, fill=MUTED)

    y = 415
    for idx, (fnum, header, rows) in enumerate(sections):
        y = section(fnum, header, rows, y)
        if fnum == "1":  # instrument tables follow the F1 section
            y = voice_grid(y, 0, grids[0])
            y = voice_grid(y, 1, grids[1])

    y = wrapped(40, y + 4,
                "No F key held: C-B records a note into the selected track while "
                "playing, or plays it live when stopped. Notes store the instrument "
                "and octave held at record time. Voice names above are the sample "
                "sources in Samples/.")

    out = pathlib.Path("docs")
    out.mkdir(exist_ok=True)
    final = img.crop((0, 0, W, min(H, y + 68)))
    final = final.resize((min(800, round(final.width * 0.8)),
                          round(final.height * 0.8)), Image.LANCZOS)
    final.convert("P", palette=Image.ADAPTIVE, colors=64).save(
        out / "InstA.png", optimize=True)
    print("wrote docs/InstA.png", final.size,
          f"{(out / 'InstA.png').stat().st_size / 1024:.1f} KB")


if __name__ == "__main__":
    main()
