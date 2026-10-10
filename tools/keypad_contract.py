#!/usr/bin/env python3
"""Parse keypad SOURCE OF TRUTH from C++ (KeypadMaps.cpp + KeypadConstants.h).

Used by make_instruction_image.py (docs/InstA.png) and gen_keypad_matrix.py
(tests/native/keypad_matrix.generated.inc). Edit the C++ — do not invent a
parallel YAML/JSON contract.
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
MAPS_CPP = ROOT / "KeypadMaps.cpp"
CONSTANTS_H = ROOT / "KeypadConstants.h"
INPUT_H = ROOT / "InputManager.h"

NOTE_MAPPERS = {
    "F1": "MapVoiceNote",
    "F2": "MapToneNote",
    "F3": "MapPatternNote",
    "F4": "MapSongNote",
}
FN_MAPPERS = {
    "F1": "MapVoiceFunction",
    "F2": "MapToneFunction",
    "F3": "MapPatternFunction",
    "F4": "MapSongFunction",
}

NOTE_ENUM_ORDER = [
    "kKeyC", "kKeyCs", "kKeyD", "kKeyDs", "kKeyE", "kKeyF",
    "kKeyFs", "kKeyG", "kKeyGs", "kKeyA", "kKeyAs", "kKeyB",
]
FUNC_ENUM_ORDER = [
    "kFuncVoice", "kFuncTone", "kFuncPattern", "kFuncSong",
]
NOTE_LABEL = {
    "kKeyC": "C", "kKeyCs": "C#", "kKeyD": "D", "kKeyDs": "D#",
    "kKeyE": "E", "kKeyF": "F", "kKeyFs": "F#", "kKeyG": "G",
    "kKeyGs": "G#", "kKeyA": "A", "kKeyAs": "A#", "kKeyB": "B",
}
FUNC_LABEL = {
    "kFuncVoice": "F1", "kFuncTone": "F2",
    "kFuncPattern": "F3", "kFuncSong": "F4",
}
# Keypad library chars for native tests.
NOTE_CHAR = {
    "kKeyC": "A", "kKeyCs": "B", "kKeyD": "C", "kKeyDs": "D",
    "kKeyE": "E", "kKeyF": "F", "kKeyFs": "G", "kKeyG": "H",
    "kKeyGs": "I", "kKeyA": "J", "kKeyAs": "K", "kKeyB": "L",
}
FUNC_CHAR = {
    "kFuncVoice": "M", "kFuncTone": "N",
    "kFuncPattern": "O", "kFuncSong": "P",
}
ARM_CHAR = {"F1": "M", "F2": "N", "F3": "O", "F4": "P"}

ROW_RE = re.compile(r"//\s*ROW:\s*(.+?)\s*\|\s*(\w+)\s*\|\s*(.+)$")
HDR_RE = re.compile(r"//\s*HDR:\s*(.+)$")
CASE_RE = re.compile(
    r"case\s+(kKey\w+|kFunc\w+)\s*:\s*return\s*\{\s*Command::(\w+)\s*,\s*(\d+)\s*\}"
)
TEMPO_RE = re.compile(
    r"constexpr\s+int\s+kTempoBpmTable\[4\]\s*=\s*\{([^}]+)\}"
)
BANK_RE = re.compile(
    r"constexpr\s+int\s+kInstrumentBank1Offset\s*=\s*(\d+)\s*;"
)
SILK_RE = re.compile(r"//\s*KEYPAD-SILK:\s*(.+?)\s*\|\s*(.+)$")
NOTE_ENUM_RE = re.compile(
    r"enum\s+NoteKey\s*:\s*int8_t\s*\{([^}]+)\}", re.MULTILINE
)


def _mapper_body(src: str, name: str) -> str:
    sig = src.index(f"InputManager::{name}(")
    end = src.index("\n}", sig)
    return src[sig:end]


def require_ascii(text: str, where: str) -> str:
    """Pillow default fonts are Latin-1-ish; InstA printout must stay ASCII."""
    bad = sorted({ch for ch in text if ord(ch) > 127})
    if bad:
        shown = ", ".join(f"{ch!r} (U+{ord(ch):04X})" for ch in bad)
        sys.exit(f"{where}: non-ASCII (Pillow-unsafe) characters: {shown}")
    return text


def parse_mapper(src: str, name: str, expect_notes: bool):
    body = _mapper_body(src, name)
    rows = []
    hdr = None
    for line in body.splitlines():
        match = ROW_RE.search(line)
        if match:
            rows.append({
                "label": require_ascii(match.group(1).strip(), f"{name} ROW label"),
                "command": require_ascii(match.group(2), f"{name} ROW command"),
                "help": require_ascii(match.group(3).strip(), f"{name} ROW help"),
            })
        match = HDR_RE.search(line)
        if match:
            hdr = require_ascii(match.group(1).strip(), f"{name} HDR")

    cases = []
    for match in CASE_RE.finditer(body):
        enum_name, command, arg = match.group(1), match.group(2), int(match.group(3))
        cases.append({"enum": enum_name, "command": command, "arg": arg})

    order = NOTE_ENUM_ORDER if expect_notes else FUNC_ENUM_ORDER
    labels = NOTE_LABEL if expect_notes else FUNC_LABEL
    chars = NOTE_CHAR if expect_notes else FUNC_CHAR

    by_enum = {c["enum"]: c for c in cases}
    missing = [e for e in order if e not in by_enum]
    if missing:
        sys.exit(f"{name}: missing cases {missing}")
    extra = [c["enum"] for c in cases if c["enum"] not in order]
    if extra:
        sys.exit(f"{name}: unexpected cases {extra}")

    documented = {row["command"] for row in rows}
    returned = {c["command"] for c in cases}
    if documented != returned:
        sys.exit(
            f"KEYPAD-DOC out of sync in {name}: "
            f"documented {sorted(documented)} vs returns {sorted(returned)}"
        )

    bindings = []
    for enum_name in order:
        case = by_enum[enum_name]
        bindings.append({
            "enum": enum_name,
            "label": labels[enum_name],
            "char": chars[enum_name],
            "command": case["command"],
            "arg": case["arg"],
        })
    return {"rows": rows, "hdr": hdr, "bindings": bindings}


def parse_note_key_order():
    """Electrical note labels from NoteKey in InputManager.h (declaration order)."""
    text = INPUT_H.read_text()
    match = NOTE_ENUM_RE.search(text)
    if not match:
        sys.exit(f"could not find enum NoteKey in {INPUT_H}")
    enums = re.findall(r"\b(kKey\w+)\b", match.group(1))
    if enums != NOTE_ENUM_ORDER:
        sys.exit(
            f"NoteKey order in InputManager.h {enums} != expected {NOTE_ENUM_ORDER}"
        )
    return [NOTE_LABEL[e] for e in enums]


def parse_silkscreen():
    """Physical keypad rows from KEYPAD-SILK lines in KeypadConstants.h."""
    rows = []
    for line in CONSTANTS_H.read_text().splitlines():
        match = SILK_RE.search(line)
        if not match:
            continue
        label = require_ascii(match.group(1).strip(), "KEYPAD-SILK label")
        keys = [
            require_ascii(k.strip(), "KEYPAD-SILK key")
            for k in match.group(2).split(",")
            if k.strip()
        ]
        if len(keys) != 4:
            sys.exit(f"KEYPAD-SILK '{label}' must list 4 keys, got {keys}")
        rows.append((label, keys))
    if len(rows) != 4:
        sys.exit(f"expected 4 KEYPAD-SILK rows in {CONSTANTS_H}, got {len(rows)}")
    return rows


def parse_constants():
    text = CONSTANTS_H.read_text()
    tempo_match = TEMPO_RE.search(text)
    bank_match = BANK_RE.search(text)
    if not tempo_match or not bank_match:
        sys.exit(f"could not parse kTempoBpmTable / kInstrumentBank1Offset in {CONSTANTS_H}")
    tempo = [int(x.strip()) for x in tempo_match.group(1).split(",")]
    if len(tempo) != 4:
        sys.exit("kTempoBpmTable must have 4 entries")
    notes = parse_note_key_order()
    silkscreen = parse_silkscreen()
    silk_notes = []
    for label, keys in silkscreen:
        if keys[0].startswith("F"):
            continue
        silk_notes.extend(keys)
    if sorted(silk_notes) != sorted(notes):
        sys.exit(
            f"KEYPAD-SILK note keys {silk_notes} are not a permutation of "
            f"NoteKey order {notes}"
        )
    # Instrument-grid display order = silkscreen note rows top→bottom.
    display_order = [notes.index(k) for k in silk_notes]
    return {
        "tempo_bpm": tempo,
        "bank1_offset": int(bank_match.group(1)),
        "notes": notes,
        "silkscreen": silkscreen,
        "note_display_order": display_order,
    }


def load_contract():
    src = MAPS_CPP.read_text()
    constants = parse_constants()
    arms = []
    for arm in ("F1", "F2", "F3", "F4"):
        note = parse_mapper(src, NOTE_MAPPERS[arm], expect_notes=True)
        func = parse_mapper(src, FN_MAPPERS[arm], expect_notes=False)
        if note["hdr"] is None:
            sys.exit(f"missing HDR: in {NOTE_MAPPERS[arm]}")
        # Print order: function rows first, then note rows (as in KEYPAD-DOC).
        sheet_rows = list(func["rows"]) + list(note["rows"])
        # Tempo help: always show live BPM table from KeypadConstants.h.
        for row in sheet_rows:
            if row["command"] == "Tempo":
                bpm = " / ".join(str(x) for x in constants["tempo_bpm"])
                row["help"] = require_ascii(f"tempo: {bpm} BPM", "Tempo help")
        arms.append({
            "arm": arm,
            "arm_char": ARM_CHAR[arm],
            "header": note["hdr"],
            "sheet_rows": sheet_rows,
            "note_bindings": note["bindings"],
            "func_bindings": func["bindings"],
        })
    return {"arms": arms, **constants}


def write_matrix_inc(path: pathlib.Path | None = None) -> pathlib.Path:
    contract = load_contract()
    out = path or (ROOT / "tests" / "native" / "keypad_matrix.generated.inc")
    lines = [
        "// GENERATED from KeypadMaps.cpp by tools/gen_keypad_matrix.py — DO NOT EDIT.",
        "// Source of truth: KeypadMaps.cpp (+ KeypadConstants.h for tempo/bank).",
        "const std::vector<Row> rows = {",
    ]
    for arm in contract["arms"]:
        lines.append(f"    // {arm['arm']} armed — {arm['header']}")
        for binding in arm["note_bindings"]:
            lines.append(
                f"    {{'{arm['arm_char']}', '{binding['char']}', "
                f"Command::{binding['command']}, {binding['arg']}}},"
            )
        for binding in arm["func_bindings"]:
            lines.append(
                f"    {{'{arm['arm_char']}', '{binding['char']}', "
                f"Command::{binding['command']}, {binding['arg']}}},"
            )
    lines.append("};")
    lines.append("")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(lines))
    return out
