"""Listen-pack: TagoClip C++ render (TagoClipRender) vs. the tagodsp reference.

Two kinds of proof in one pack:
  1. Golden pairs (Python reference vs. C++ render) for all 7 golden/refs
     configs. These should sound and measure identical; the point is to
     confirm the JUCE port didn't lose anything the golden test's 5e-6
     tolerance might not catch by ear (e.g. denormal ringing, dither noise).
  2. Clone-check segments: the real FL Studio Fruity Soft Clipper bounce
     (measure/exports/) vs. the C++ render of the same dry input. This is
     the actual "does it sound like Fruity" proof, not just Python-parity.

Run with the tagodsp venv python:
    ~/Documents/tagodsp/.venv/bin/python scripts/build_listenpack.py
"""

import subprocess
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import soundfile as sf

sys.path.insert(0, str(Path.home() / "Documents/tagodsp/python"))
sys.path.insert(0, str(Path.home() / "Documents/tagodsp"))

from tagodsp.utils.gain import rms_db  # noqa: E402
from tools.listenpack import ListenPack  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
BIN = ROOT / "build/TagoClipRender_artefacts/Release/TagoClipRender"
REFS = ROOT / "golden/refs"
MEASURE = ROOT / "measure/exports"
SR = 44100
TARGET_RMS_DB = -18.0
# scipy.signal.resample_poly truncates the up stage's pre-ring at t < 0 before
# it reaches the decimation filter; the streaming C++ resampler correctly
# feeds it through instead. Only the first ~10 samples after reset differ
# (verified 2026-07-15, see golden/compare.py EDGE_SKIP). Matched here so the
# listen pack doesn't open with a loud, meaningless click at t=0.
EDGE_SKIP = 64

# name, curve, threshold (Fruity steps), fruity render file, segment start/dur
FRUITY_SEGMENTS = [
    ("sine_1k_hot", "fl", 100, "full_thr_default", 17.12, 4.0),
    ("sine_5k_hot", "fl", 67, "full_thr_low30", 30.83, 4.0),
    ("transients", "fl", 100, "full_thr_default", 37.69, 4.0),
]


def loudness_match(*signals: np.ndarray, target_db: float = TARGET_RMS_DB) -> tuple:
    """Apply one shared gain (from the first signal's RMS) to all signals,
    so relative level differences between them are preserved."""
    ref_db = rms_db(signals[0])
    gain = 10.0 ** ((target_db - ref_db) / 20.0)
    return tuple(s * gain for s in signals), gain


def run_cli(in_path: Path, out_path: Path, curve: str, threshold: int, drive: float,
            os_factor: int, output_db: float, mono_low: float, delta: int) -> None:
    cmd = [str(BIN), str(in_path), str(out_path), curve, str(threshold), str(drive),
           str(os_factor), str(output_db), str(mono_low), str(delta), "512"]
    subprocess.run(cmd, check=True, capture_output=True)


def spectrum_delta_plot(pack: ListenPack, name: str, a: np.ndarray, b: np.ndarray, sr: int, title: str) -> None:
    from scipy.signal import welch

    fig, axes = plt.subplots(2, 1, figsize=(9, 6), sharex=True)
    for label, x in (("python", a), ("cpp", b)):
        f, pxx = welch(np.asarray(x, np.float64).mean(axis=-1) if x.ndim > 1 else x, fs=sr, nperseg=8192)
        axes[0].semilogx(f[1:], 10 * np.log10(pxx[1:] + 1e-20), label=label, alpha=0.8)
    axes[0].set_ylabel("PSD [dB]")
    axes[0].set_title(title)
    axes[0].grid(True, which="both", alpha=0.3)
    axes[0].legend()

    d = np.abs(a - b)
    d_db = 20 * np.log10(np.maximum(d, 1e-12))
    t = np.arange(len(d)) / sr
    axes[1].plot(t, d_db.max(axis=-1) if d_db.ndim > 1 else d_db, linewidth=0.6, color="crimson")
    axes[1].set_ylabel("|delta| [dBFS]")
    axes[1].set_xlabel("time [s]")
    axes[1].grid(True, alpha=0.3)
    pack.add_plot(name, fig)


def main():
    if not BIN.exists():
        sys.exit(f"missing {BIN}, build TagoClipRender first")
    if not REFS.exists():
        sys.exit(f"missing {REFS}, run golden/make_reference.py first")

    manifest = __import__("json").loads((REFS / "manifest.json").read_text())
    configs = manifest["configs"]

    pack = ListenPack("tagoclip-cpp-vs-python", root=str(ROOT / "listening"))
    deltas_dir = pack.dir / "deltas"
    deltas_dir.mkdir(exist_ok=True)

    print("--- golden pairs (python reference vs. C++ render) ---")
    out_dir = ROOT / "golden" / "out"
    out_dir.mkdir(exist_ok=True)
    for cfg in configs:
        name = cfg["name"]
        a = sf.read(REFS / f"{name}.wav", dtype="float64")[0]
        out_path = out_dir / f"{name}.wav"
        run_cli(REFS / "input.wav", out_path, cfg["curve"], cfg["threshold"], cfg["drive"],
                cfg["os"], cfg["output"], cfg["monolow"], cfg["delta"])
        b = sf.read(out_path, dtype="float64")[0]
        n = min(len(a), len(b))
        a, b = a[EDGE_SKIP:n], b[EDGE_SKIP:n]

        (a_m, b_m), gain = loudness_match(a, b)
        pack.add_pair(name, a_m, b_m, SR, label_a="python", label_b="cpp")

        delta = a - b
        (delta_m,), delta_gain = loudness_match(delta, target_db=-24.0)
        sf.write(deltas_dir / f"{name}_delta.wav", delta_m.astype(np.float32), SR)

        peak_delta = float(np.max(np.abs(delta)))
        print(f"{name}: peak delta {peak_delta:.2e}, listen gain {20*np.log10(gain):+.1f} dB, "
              f"delta gain {20*np.log10(delta_gain):+.1f} dB")
        spectrum_delta_plot(pack, f"spectrum_{name}", a, b, SR,
                             f"{name}: python vs cpp (peak delta {peak_delta:.1e})")

    print("\n--- clone check (real Fruity bounce vs. C++ render of the same dry input) ---")
    dry = sf.read(MEASURE / "full_dry.wav", dtype="float64")[0][:, 0]
    for seg_name, curve, thr, render_file, start, dur in FRUITY_SEGMENTS:
        s0, s1 = int(start * SR), int((start + dur) * SR)
        fruity = sf.read(MEASURE / f"{render_file}.wav", dtype="float64")[0][:, 0][s0:s1]
        dry_seg = dry[s0:s1]

        tmp_in = deltas_dir.parent / f"_tmp_{seg_name}_dry.wav"
        tmp_out = deltas_dir.parent / f"_tmp_{seg_name}_cpp.wav"
        # dry can exceed +-1.0 (measured input range +-3.63), so the temp file
        # must be float, not soundfile's default 16-bit PCM, or it gets
        # clipped/quantized before the CLI ever sees it.
        sf.write(tmp_in, np.column_stack([dry_seg, dry_seg]).astype(np.float32), SR, subtype="FLOAT")
        run_cli(tmp_in, tmp_out, curve, thr, 0.0, 1, 0.0, 0.0, 0)
        ours = sf.read(tmp_out, dtype="float64")[0][:, 0][: len(fruity)]
        tmp_in.unlink()
        tmp_out.unlink()

        n = min(len(fruity), len(ours))
        fruity, ours = fruity[:n], ours[:n]
        (f_m, o_m), gain = loudness_match(fruity, ours)
        item = f"clone_{seg_name}"
        pack.add_pair(item, f_m, o_m, SR, label_a="fruity_original", label_b="tagoclip_cpp")

        delta = fruity - ours
        (delta_m,), delta_gain = loudness_match(delta, target_db=-24.0)
        sf.write(deltas_dir / f"{item}_delta.wav", delta_m.astype(np.float32), SR)

        peak_delta = float(np.max(np.abs(delta)))
        print(f"{seg_name}: peak delta {peak_delta:.2e}, listen gain {20*np.log10(gain):+.1f} dB")
        spectrum_delta_plot(pack, f"spectrum_clone_{seg_name}", fruity, ours, SR,
                             f"clone check {seg_name}: fruity vs cpp (peak delta {peak_delta:.1e})")

    settings = pack.dir / "settings.txt"
    settings.write_text(
        "TagoClip: C++ render (TagoClipRender) vs. tagodsp Python reference.\n"
        f"Target listening RMS: {TARGET_RMS_DB} dB (shared gain per pair, relative levels preserved).\n"
        "Delta bounces normalized separately to -24 dB RMS, gain noted per item above.\n\n"
        "Golden pairs: A = python (golden/refs), B = cpp (golden/out), same synthetic test signal.\n"
        f"First {EDGE_SKIP} samples cropped from all golden pairs and their deltas: known,\n"
        "understood scipy-vs-streaming-resampler pre-ring truncation right after reset\n"
        "(see golden/compare.py EDGE_SKIP), not an audible issue, would just be a click at t=0.\n\n"
        "Clone-check pairs: A = real FL Studio Fruity Soft Clipper bounce (measure/exports),\n"
        "B = TagoClip C++ render of the same dry input at the matching threshold.\n"
    )

    page = pack.audition_page(title="TagoClip: C++ vs. Python, Clone-Check gegen Fruity")
    print(f"\nListenpack: {pack.dir}")
    print(f"Audition:   {page}")


if __name__ == "__main__":
    main()
