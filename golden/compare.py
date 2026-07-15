"""Golden comparison: TagoClipRender (C++) vs the tagodsp references.

Runs the render CLI for every config in golden/refs/manifest.json, compares
against the Python reference renders, and checks block-size invariance of the
resampler state (same input, different block sizes, bit-identical output).
Exits nonzero on any failure.

Run with the tagodsp venv python:
    ~/Documents/tagodsp/.venv/bin/python golden/compare.py <path-to-TagoClipRender>
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

HERE = Path(__file__).resolve().parent
REFS = HERE / "refs"
OUT = HERE / "out"

BLOCK_SWEEP = (64, 512, 1000, 4096)
SWEEP_CONFIG = "os8_default"

# scipy.signal.resample_poly truncates the up stage's pre-ring at t < 0 before
# it reaches the decimation filter; a streaming resampler correctly feeds it
# through. Only the first ~10 samples after reset differ (verified 2026-07-15,
# per-stage streaming replicas match scipy to 4e-16), so the comparison starts
# after a short settle window.
EDGE_SKIP = 64


def run_cli(binary: Path, cfg: dict, out_path: Path, block: int) -> str:
    cmd = [
        str(binary),
        str(REFS / "input.wav"),
        str(out_path),
        cfg["curve"],
        str(cfg["threshold"]),
        str(cfg["drive"]),
        str(cfg["os"]),
        str(cfg["output"]),
        str(cfg["monolow"]),
        str(cfg["delta"]),
        str(block),
    ]
    result = subprocess.run(cmd, capture_output=True, text=True, check=True)
    return result.stdout.strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path, help="path to the TagoClipRender binary")
    args = parser.parse_args()

    manifest = json.loads((REFS / "manifest.json").read_text())
    tol = manifest["tolerance"]
    OUT.mkdir(exist_ok=True)
    failures = []

    print(f"tolerance {tol:.1e}\n")
    for cfg in manifest["configs"]:
        out_path = OUT / f"{cfg['name']}.wav"
        info = run_cli(args.binary, cfg, out_path, 512)
        ref = sf.read(REFS / f"{cfg['name']}.wav", dtype="float64")[0]
        got = sf.read(out_path, dtype="float64")[0]
        n = min(len(ref), len(got))
        delta = np.abs(ref[EDGE_SKIP:n] - got[EDGE_SKIP:n])
        peak = float(np.max(delta))
        pos = EDGE_SKIP + int(np.argmax(np.max(delta, axis=1)))
        ok = peak < tol
        if not ok:
            failures.append(cfg["name"])
        print(
            f"{'PASS' if ok else 'FAIL'} {cfg['name']:<15} max delta {peak:.2e} "
            f"@ sample {pos} ({info})"
        )

    # Block-size invariance: resampler and filter state must not depend on
    # how the stream is chopped up.
    sweep_cfg = next(c for c in manifest["configs"] if c["name"] == SWEEP_CONFIG)
    renders = []
    for block in BLOCK_SWEEP:
        out_path = OUT / f"sweep_{block}.wav"
        run_cli(args.binary, sweep_cfg, out_path, block)
        renders.append(sf.read(out_path, dtype="float64")[0])
    identical = all(np.array_equal(renders[0], r) for r in renders[1:])
    if not identical:
        failures.append("block_sweep")
    print(f"\n{'PASS' if identical else 'FAIL'} block sweep {BLOCK_SWEEP}: "
          f"{'bit-identical' if identical else 'outputs differ'}")

    if failures:
        print(f"\nFAILED: {', '.join(failures)}")
        sys.exit(1)
    print("\nall golden checks passed")


if __name__ == "__main__":
    main()
