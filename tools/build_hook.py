# PlatformIO PRE-BUILD hook: keeps Samples/*.h + SampleGains.h generated from
# the pristine sources before every firmware compile.
#
# Wired via `extra_scripts = pre:tools/build_hook.py` in platformio.ini (not
# currently enabled). The full pipeline (gen_all.sh: python pristine import ->
# C++ verify gate -> numpy loops/fixups -> header+gain generation -> C++
# verifygen round-trip gate) runs ONLY when its inputs changed; the stamp
# covers: git HEAD revision (the pristine headers), every Samples_src WAV,
# the pipeline scripts, and the sample rate config. So a firmware build is
# never surprised by stale generated headers, and an unchanged tree costs
# one hash to check.
#
# Run standalone for testing: python tools/build_hook.py
import hashlib
import pathlib
import subprocess
import sys

# PlatformIO runs this via SCons SConscript(), where __file__ is NOT defined
# but `env` is exported; standalone runs have neither guarantee. Derive the
# project root from whichever context we're in.
try:
    ROOT = pathlib.Path(env.subst("$PROJECT_DIR")).resolve()  # noqa: F821
except NameError:
    ROOT = pathlib.Path(__file__).resolve().parents[1]
STAMP = ROOT / ".pipeline" / "samples.stamp"

WATCHED_SCRIPTS = [
    "tools/gen_all.sh",
    "tools/import_pristine.py",
    "tools/loops.py",
    "tools/gen_headers.py",
    "tools/samplelib.py",
    "tools/make_samples.cpp",
    "tests/native/WavWriter.h",
    "AudioConfig.h",
]


def _file_tag(path):
    # CONTENT hash, not mtime: the pipeline deterministically rewrites
    # outputs (loops WAVs) with identical bytes, so an mtime stamp could
    # never match after its own run and would rebuild every single time.
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    return f"{path}:{digest}"


def fingerprint():
    h = hashlib.sha256()
    try:
        head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT,
                              capture_output=True, text=True, check=True).stdout
        h.update(f"git:{head.strip()}\n".encode())
    except Exception as exc:  # no git -> fingerprint failure, pipeline will report
        h.update(f"git:ERROR:{exc}\n".encode())
    for rel in WATCHED_SCRIPTS:
        p = ROOT / rel
        h.update((f"{p}:MISSING" if not p.exists() else _file_tag(p)).encode() + b"\n")
    src = ROOT / "Samples_src"
    for wav in sorted(src.rglob("*.wav")) if src.is_dir() else []:
        h.update(_file_tag(wav).encode() + b"\n")
    return h.hexdigest()


def _outputs_present():
    return (ROOT / "SampleGains.h").exists() and (ROOT / "Samples" / "bass1.h").exists()


def ensure_samples():
    current = fingerprint()
    if _outputs_present() and STAMP.exists() and STAMP.read_text().strip() == current:
        return  # generated headers are provably fresh; build proceeds untouched
    print("[build_hook] sample inputs changed -> running pipeline (gen_all.sh)")
    res = subprocess.run(["sh", "tools/gen_all.sh"], cwd=ROOT)
    if res.returncode != 0:
        # Fail the build loudly: compiling stale/partial generated sample
        # headers is exactly the drift this hook exists to prevent.
        sys.exit("[build_hook] sample pipeline FAILED - fix before building "
                 f"(env: {sys.executable} needs numpy+scipy; see "
                 "tools/requirements.txt)")
    STAMP.parent.mkdir(parents=True, exist_ok=True)
    # Re-fingerprint AFTER the pipeline: stamp what the tree actually looks
    # like now (the pipeline may legitimately rewrite outputs).
    STAMP.write_text(fingerprint() + "\n")
    print("[build_hook] samples regenerated and gates passed")


ensure_samples()
