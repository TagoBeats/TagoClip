"""Render the golden reference WAVs with the tagodsp prototypes.

Builds the shared test input and one reference render per config in CONFIGS.
The C++ side (tools/render_cli.cpp) must reproduce every reference within the
tolerance in the manifest, see compare.py. Signal chain, in this order and
mirrored 1:1 by plugin/ClipEngine.h:

    mono-low (LR4, pre clipper) -> drive -> curve with oversampling
    -> optional delta (wet minus driven dry) -> output gain

Run with the tagodsp venv python:
    ~/Documents/tagodsp/.venv/bin/python golden/make_reference.py
"""

import json
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

sys.path.insert(0, str(Path.home() / "Documents/tagodsp/python"))

from tagodsp.distortion.clipper import Clipper  # noqa: E402
from tagodsp.stereo.mono_low import MonoLow  # noqa: E402
from tagodsp.utils.gain import db_to_lin  # noqa: E402

SR = 44100
REFS = Path(__file__).resolve().parent / "refs"
TOLERANCE = 5e-6

# name, curve, threshold (Fruity steps), drive_db, os, output_db, mono_low, delta
CONFIGS = [
    {"name": "fruity_1to1",    "curve": "fl",   "threshold": 100, "drive": 0.0, "os": 1, "output": 0.0,  "monolow": 0.0,  "delta": 0},
    {"name": "os8_default",    "curve": "fl",   "threshold": 100, "drive": 6.0, "os": 8, "output": 0.0,  "monolow": 0.0,  "delta": 0},
    {"name": "os4_hard",       "curve": "hard", "threshold": 64,  "drive": 9.0, "os": 4, "output": -3.0, "monolow": 0.0,  "delta": 0},
    {"name": "tanh_os8",       "curve": "tanh", "threshold": 51,  "drive": 8.0, "os": 8, "output": -2.0, "monolow": 0.0,  "delta": 0},
    {"name": "monolow_only",   "curve": "fl",   "threshold": 127, "drive": 0.0, "os": 1, "output": 0.0,  "monolow": 0.67, "delta": 0},
    {"name": "chain_808glue",  "curve": "fl",   "threshold": 84,  "drive": 6.0, "os": 8, "output": -1.0, "monolow": 0.67, "delta": 0},
    {"name": "delta_listen",   "curve": "fl",   "threshold": 84,  "drive": 6.0, "os": 8, "output": 0.0,  "monolow": 0.0,  "delta": 1},
]


def make_input() -> np.ndarray:
    """5 s stereo: detuned 808 hits, noise hats, hot 5 kHz tone (alias probe)."""
    rng = np.random.default_rng(42)
    n = 5 * SR
    x = np.zeros((n, 2))

    for start in (0.0, 0.5, 1.0, 1.5):
        i0 = int(start * SR)
        dur = int(0.45 * SR)
        tt = np.arange(dur) / SR
        env = np.exp(-tt * 4.0)
        freq = 40.0 + 30.0 * np.exp(-tt * 8.0)
        phase = 2.0 * np.pi * np.cumsum(freq) / SR
        x[i0 : i0 + dur, 0] += 0.9 * env * np.sin(phase * 0.995)
        x[i0 : i0 + dur, 1] += 0.9 * env * np.sin(phase * 1.005)

    for start in np.arange(2.0, 3.4, 0.1):
        i0 = int(start * SR)
        dur = int(0.06 * SR)
        env = np.exp(-np.arange(dur) / SR * 60.0)
        x[i0 : i0 + dur, 0] += 0.4 * env * rng.standard_normal(dur)
        x[i0 : i0 + dur, 1] += 0.4 * env * rng.standard_normal(dur)

    i0, i1 = int(3.5 * SR), int(4.8 * SR)
    tt = np.arange(i1 - i0) / SR
    x[i0:i1, 0] += 0.95 * np.sin(2.0 * np.pi * 5000.0 * tt)
    x[i0:i1, 1] += 0.95 * np.sin(2.0 * np.pi * 5000.0 * tt + 0.3)

    return x


def render(cfg: dict, x: np.ndarray) -> np.ndarray:
    y = x.copy()
    if cfg["monolow"] >= 0.03:
        y = MonoLow(freq=20.0 * 20.0 ** cfg["monolow"], sr=SR).process(y)
    clip = Clipper(cfg["curve"], cfg["threshold"] / 128.0, cfg["os"], drive_db=cfg["drive"])
    wet = np.column_stack([clip.process(y[:, c]) for c in range(2)])
    if cfg["delta"]:
        wet = wet - y * db_to_lin(cfg["drive"])
    return wet * db_to_lin(cfg["output"])


def main():
    REFS.mkdir(parents=True, exist_ok=True)

    input_path = REFS / "input.wav"
    sf.write(input_path, make_input().astype(np.float32), SR, subtype="FLOAT")
    # Re-read so the references start from the same float32 samples the CLI reads.
    x = sf.read(input_path, dtype="float64")[0]

    for cfg in CONFIGS:
        ref = render(cfg, x)
        sf.write(REFS / f"{cfg['name']}.wav", ref, SR, subtype="DOUBLE")
        print(f"ref {cfg['name']}: peak {np.max(np.abs(ref)):.4f}")

    manifest = {"sr": SR, "tolerance": TOLERANCE, "input": "input.wav", "configs": CONFIGS}
    (REFS / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"wrote {len(CONFIGS)} references to {REFS}")


if __name__ == "__main__":
    main()
