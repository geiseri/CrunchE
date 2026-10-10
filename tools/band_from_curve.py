#!/usr/bin/env python3
"""Derive SPEAKER_BAND_HZ from a datasheet: response curve, THD curve, or F0/Qts.

Prints the two band edges with the reasoning, plus a pasteable constant line.
Pure stdlib; run it by hand whenever a transducer changes (README, "Deriving
the band from a spec sheet").

  python tools/band_from_curve.py response.csv [--thd thd.csv]
  python tools/band_from_curve.py --f0 85 [--qts 0.9]
  python tools/band_from_curve.py "80,-14 160,-7 250,-3 500,0 8000,0 12k,-3"

Positional/CSV: "<freq> <db>" (or comma) per line; freq accepts 12k/12000.
Reference = mean dB over 500-2000 Hz (midband plateau). Edges = where the
curve drops 3 dB below it. An unbaffled driver rolls off 12 dB/oct below Fs
(excursion is constant there, SPL ~ f^2), so the -3 dB corner sits at
Fs*sqrt(x), x = Qts^-2-dependent root: only Qts~0.7 puts it AT F0; peaky
drivers cross below F0, damped ones far above -- with a curve, trust the
curve; with F0 alone, pass --qts if you have it.

THD veto (audibility is level-, order-, and frequency-dependent; Fielder &
Benjamin masking data via Audioholics): crossings are judged at 5 % below
500 Hz and 3 % above; the lowest one above lo caps `hi`. LF peaks (the
suspension hump at/below Fs) never veto `hi` -- they are reported as the
SATURATE advisory in loops.py instead. Re-check any THD spec at the drive
level you actually run (kMasterDiv-limited << rated W).
"""
import argparse
import math
import re
import sys


def parse_freq(tok):
    tok = tok.strip().lower().rstrip("hz").strip()
    m = re.fullmatch(r"(\d+(?:\.\d+)?)([mk]?)", tok)
    if not m:
        raise ValueError(f"bad frequency: {tok!r}")
    return float(m.group(1)) * (1e3 if m.group(2) == "k" else 1.0)


def parse_pairs(text):
    """Scan any (freq, dB) pairs: one per CSV line or several inline, comma
    or space separated. Freq accepts 12k/12khz/12000; dB accepts +1/-3.5."""
    pairs = re.findall(
        r"(\d+(?:\.\d+)?[mk]?(?:hz)?)\s*[,\s]\s*([-+]?\d+(?:\.\d+)?)\s*(?=$|[,\s])",
        text, re.I)
    return sorted((parse_freq(f), float(d)) for f, d in pairs)


def read_source(arg):
    if arg and not arg.startswith("-") and arg.lower().endswith(".csv"):
        return open(arg).read()
    return arg  # inline string (or stdin text)


def interp_cross(pts, i_lo, i_hi, level):
    """Log-f interpolation of the crossing between consecutive points."""
    f1, d1 = pts[i_lo]
    f2, d2 = pts[i_hi]
    if abs(d2 - d1) < 1e-9:
        return f1
    t = (level - d1) / (d2 - d1)
    return math.exp(math.log(f1) + t * (math.log(f2) - math.log(f1)))


def edges_from_response(pts, drop=3.0):
    fs_ = [f for f, _ in pts]
    band = [d for f, d in pts if 500 <= f <= 2000]
    ref = sum(band) / len(band) if band else max(d for _, d in pts)
    thr = ref - drop
    above = [i for i, (f, d) in enumerate(pts) if d >= thr]
    lo = interp_cross(pts, above[0] - 1, above[0], thr) if above[0] else fs_[0]
    last_ok = above[-1]
    hi = (interp_cross(pts, last_ok, last_ok + 1, thr)
          if last_ok + 1 < len(pts) else fs_[-1])
    warn = []
    if pts[0][1] >= thr:
        warn.append("curve already within 3 dB at its lowest point: real lo "
                    "is below the data; use F0/Qts estimate as the floor")
    if pts[-1][1] >= thr:
        warn.append("curve still within 3 dB at its highest point: real hi is "
                    "above the data (THD veto or hearing will bound it)")
    return lo, hi, ref, warn


def corner_from_qts(qts):
    u = (-(2 - 1 / qts**2) + math.sqrt((2 - 1 / qts**2) ** 2 + 4)) / 2
    return math.sqrt(u)


def thd_veto(thd, lo, hi):
    capped, advisories = hi, []
    for f, t in thd:
        if f <= lo or f > capped:
            continue
        thr = 5.0 if f < 500 else 3.0
        if t >= thr:
            capped = f  # lowest offending crossing wins
            break
    lf = [(f, t) for f, t in thd if f < 500]
    if lf:
        f, t = max(lf, key=lambda p: p[1])
        advisories.append(f"LF THD {t:.0f} % at {f:.0f} Hz (suspension hump "
                          "near Fs): fundamentals there are dirty - candidate "
                          "set for SATURATE/SAT_DRIVE in loops.py")
    if any(t >= 8 for _, t in lf):
        advisories.append("masking tolerates LF distortion (~5 % at the 2nd "
                          "harmonic, more at loud SPL) - do NOT raise lo for it")
    return capped, advisories


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("response", nargs="?", help="response CSV path or inline "
                    'list: "80,-14 250,-3 ... " (freq,dB)')
    ap.add_argument("--thd", help="THD CSV path or inline (freq,percent)")
    ap.add_argument("--f0", type=float, help="free-air resonance (datasheet "
                    "typ) when no response curve exists")
    ap.add_argument("--qts", type=float, default=0.707, help="total Q (default "
                    "0.707: only then is -3 dB exactly at F0)")
    a = ap.parse_args()
    if a.response == "-":
        a.response = sys.stdin.read()
    elif not a.response and not a.f0:
        a.response = sys.stdin.read()

    lo = hi = None
    print("== CrunchE speaker band derivation ==")
    if a.response:
        pts = parse_pairs(read_source(a.response))
        if len(pts) < 3:
            sys.exit("need >=3 (freq, dB) points")
        lo, hi, ref, warn = edges_from_response(pts)
        print(f"response: {len(pts)} pts, midband ref {ref:+.1f} dB")
        for w in warn:
            print("note:", w)
    if a.f0:
        x = corner_from_qts(a.qts)
        est = a.f0 * x
        print(f"F0={a.f0:.0f} Hz, Qts={a.qts:.2f} -> -3 dB at {x:.2f}*F0 = "
              f"{est:.0f} Hz" + ("" if lo else "  (no curve; using this as lo)"))
        lo = lo or est
        if hi is None:
            hi = 8000.0  # no upper data at all: mid-size default, README table
            print("note: no response/THD curve - hi=8000 assumed; refine from "
                  "the datasheet's response plot or the class table in README")
    if a.thd:
        thd = parse_pairs(read_source(a.thd))
        hi2, advisories = thd_veto(thd, lo, hi)
        if hi2 < hi:
            print(f"THD veto: {hi:.0f} -> {hi2:.0f} Hz")
        hi = hi2
        for ad in advisories:
            print("advisory:", ad)
    print(f"\nSPEAKER_BAND_HZ = ({sig(lo)}, {sig(hi)})")
    print("\nCheck on the gates' side: re-run `sh tools/gen_all.sh`, then the "
          "native tests, and listen - a voice one octave below lo radiates "
          "~1/16 power (-12 dB): that is SATURATE/REGISTER_SHIFT territory.")


def sig(v):
    return float(f"{v:.3g}")


if __name__ == "__main__":
    main()
