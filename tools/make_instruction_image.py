#!/usr/bin/env python3
"""Render docs/InstA.png — standalone keypad printout from firmware C++.

Source of truth:
  KeypadMaps.cpp          — key → Command + KEYPAD-DOC sheet wording
  KeypadConstants.h       — tempo/bank + KEYPAD-SILK membrane layout
  InputManager.h          — NoteKey electrical order
  Voice.cpp               — pitched instrument names on the F1 grids

    ~/.platformio/penv/bin/python tools/make_instruction_image.py
"""
from __future__ import annotations

import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from keypad_contract import load_contract  # noqa: E402

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.exit("need Pillow: ~/.platformio/penv/bin/python -m pip install pillow")

ROOT = pathlib.Path(__file__).resolve().parents[1]

TITLE = "Crunch-E Keypad Quick-ref"
BLURB = "F1-F4 arm a function (its LED stays lit); the next key applies it."
FOOTER = (
    "No F key held: C-B records a note into the selected track while playing, "
    "or plays it live when stopped. Notes store the instrument and octave held "
    "at record time. Voice names above are the sample sources in Samples/."
)


def parse_instruments(bank1_offset: int):
    voice = (ROOT / "Voice.cpp").read_text()
    table = voice[voice.index("instrumentSources[] = {"):]
    table = table[:table.index("};")]
    names = re.findall(r"\{(\w+),\s*kGain_\w+\}", table)
    pitched_bank0 = bank1_offset - 2
    expected = pitched_bank0 + 9
    if len(names) != expected:
        sys.exit(
            f"instrumentSources has {len(names)} entries, expected {expected} "
            f"(kInstrumentBank1Offset={bank1_offset})"
        )
    return names


def build_grids(names, notes, bank1_offset: int, display_order):
    bank0 = {"C": "drums", "C#": "sfx"}
    for index in range(bank1_offset - 2):
        bank0[notes[2 + index]] = names[index]
    bank1 = {}
    bank1_count = len(names) - (bank1_offset - 2)
    for index in range(bank1_count):
        bank1[notes[index]] = names[(bank1_offset - 2) + index]
    for key in notes[bank1_count:]:
        bank1[key] = "silent"
    return [
        [(notes[i], bank[notes[i]]) for i in display_order]
        for bank in (bank0, bank1)
    ]


WIDTH, HEIGHT = 1000, 1560
BG = (247, 246, 242)
INK = (28, 28, 32)
MUTED = (110, 108, 100)
ACCENT = (216, 96, 40)
KEY_BG = (254, 252, 247)
KEY_EDGE = (190, 186, 176)


def find_font(size, bold=False):
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

img = Image.new("RGB", (WIDTH, HEIGHT), BG)
draw = ImageDraw.Draw(img)


def badge(posX, posY, text, width=34, fill=ACCENT, fg=(255, 255, 255)):
    draw.rounded_rectangle([posX, posY, posX + width, posY + 24], 6, fill=fill)
    bbox = draw.textbbox((0, 0), text, font=F_BADGE)
    draw.text((posX + (width - (bbox[2] - bbox[0])) / 2 - bbox[0],
               posY + (24 - (bbox[3] - bbox[1])) / 2 - bbox[1]), text,
              font=F_BADGE, fill=fg)


def text(posX, posY, content, font=F_NOTE, fill=INK):
    draw.text((posX, posY), content, font=font, fill=fill)


def wrapped(posX, posY, content, font=F_SMALL, fill=MUTED, max_w=WIDTH - 80,
            line_h=18):
    words = content.split()
    line = []
    for word in words:
        trial = " ".join(line + [word])
        if line and draw.textlength(trial, font=font) > max_w:
            text(posX, posY, " ".join(line), font=font, fill=fill)
            posY += line_h
            line = [word]
        else:
            line = trial.split()
    if line:
        text(posX, posY, " ".join(line), font=font, fill=fill)
        posY += line_h
    return posY


def section(fnum, header, rows, posY):
    badge(40, posY + 1, "F" + fnum)
    text(84, posY, header, font=F_HEAD)
    posY += 34
    for label, desc in rows:
        text(84, posY, label, font=F_LBL, fill=ACCENT)
        text(214, posY, desc, font=F_NOTE)
        posY += 29
    return posY + 12


def voice_grid(posY, bank, cells):
    text(84, posY, f"bank {bank}  (F1 + note):", font=F_NOTE, fill=ACCENT)
    posY += 25
    for row in range(3):
        for col in range(4):
            note, name = cells[row * 4 + col]
            posX = 84 + col * 224
            bbox = draw.textbbox((0, 0), note, font=F_SMALL)
            text(posX, posY, note, font=F_SMALL, fill=INK)
            text(posX + (bbox[2] - bbox[0]) + 8, posY, name, font=F_SMALL,
                 fill=MUTED)
        posY += 22
    return posY + 8


def main():
    contract = load_contract()
    grids = build_grids(
        parse_instruments(contract["bank1_offset"]),
        contract["notes"],
        contract["bank1_offset"],
        contract["note_display_order"],
    )

    text(40, 30, TITLE, font=F_TITLE)

    COLX = [170 + 202 * index for index in range(4)]
    KW, KH, GAP = 180, 60, 10
    TOP = 100
    for index, (label, keys) in enumerate(contract["silkscreen"]):
        posY = TOP + index * (KH + GAP)
        text(24, posY + 24, label, font=F_SMALL, fill=MUTED)
        for col, key in enumerate(keys):
            posX = COLX[col]
            is_func = keys[0].startswith("F")
            draw.rounded_rectangle(
                [posX, posY, posX + KW, posY + KH], 10,
                fill=(ACCENT if is_func else KEY_BG),
                outline=KEY_EDGE, width=2)
            bbox = draw.textbbox((0, 0), key, font=F_KEY)
            draw.text((posX + (KW - bbox[2] + bbox[0]) / 2 - bbox[0],
                       posY + (KH - bbox[3] + bbox[1]) / 2 - bbox[1]), key,
                      font=F_KEY,
                      fill=(255, 255, 255) if is_func else INK)

    posY = TOP + 4 * (KH + GAP) + 6
    text(COLX[0], posY, BLURB, font=F_SMALL, fill=MUTED)

    posY = 415
    for arm in contract["arms"]:
        fnum = arm["arm"][1]
        rows = [(r["label"], r["help"]) for r in arm["sheet_rows"]]
        posY = section(fnum, arm["header"], rows, posY)
        if arm["arm"] == "F1":
            posY = voice_grid(posY, 0, grids[0])
            posY = voice_grid(posY, 1, grids[1])

    posY = wrapped(40, posY + 4, FOOTER)

    out = ROOT / "docs"
    out.mkdir(exist_ok=True)
    final = img.crop((0, 0, WIDTH, min(HEIGHT, posY + 68)))
    final = final.resize((min(800, round(final.width * 0.8)),
                          round(final.height * 0.8)), Image.LANCZOS)
    final.convert("P", palette=Image.ADAPTIVE, colors=64).save(
        out / "InstA.png", optimize=True)
    print("wrote docs/InstA.png", final.size,
          f"{(out / 'InstA.png').stat().st_size / 1024:.1f} KB")


if __name__ == "__main__":
    main()
