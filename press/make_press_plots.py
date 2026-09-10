"""English-labelled versions of the Fruity Soft Clipper measurement plots.

Same data and same maths as measure/make_analysis_plots.py, which stays the
source of truth and is read-only. This one exists because the originals are
labelled in German and the plots get posted to English-speaking forums.

Reads the untouched bounces from measure/exports/ and writes into press/.
"""

from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import soundfile as sf

BASE = Path(__file__).parent.parent
SRC = BASE / "measure"
OUT = BASE / "press"
OUT.mkdir(exist_ok=True)

DPI = 200  # forums downscale, so give them headroom


def curve_exp(x, t):
    ax = np.abs(x)
    return np.sign(x) * np.where(ax <= t, ax, 1 - (1 - t) * np.exp(-(ax - t) / (1 - t)))


def main() -> None:
    dry, sr = sf.read(SRC / "exports/full_dry.wav")
    dry = dry[:, 0]
    n_ramp = int(8.0 * sr)
    x = dry[:n_ramp]

    # Plot 1: transfer curves, measurement as dots, formula as lines
    fig, ax1 = plt.subplots(figsize=(8, 6))
    cases = [
        ("full_thr_high", 127 / 128, "max (127/128)", "tab:red"),
        ("full_thr_default", 100 / 128, "default (100/128)", "tab:blue"),
        ("full_thr_low30", 67 / 128, "'30%' (67/128)", "tab:green"),
    ]
    xi = np.linspace(-3.6, 3.6, 2000)
    for name, t, label, color in cases:
        y = sf.read(SRC / f"exports/{name}.wav")[0][:n_ramp, 0]
        ax1.plot(x[::40], y[::40], ".", ms=3, alpha=0.4, color=color)
        ax1.plot(xi, curve_exp(xi, t), "-", lw=1.2, color=color, label=f"Threshold {label}")
    ax1.set_xlabel("Input")
    ax1.set_ylabel("Output")
    ax1.set_title(
        "Fruity Soft Clipper transfer curve\n"
        "dots = measured, lines = y = sign(x)(1-(1-t)exp(-(|x|-t)/(1-t)))"
    )
    ax1.legend()
    ax1.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(OUT / "clipper_01_transfer_curves.png", dpi=DPI)
    plt.close(fig)

    # Plot 2: knee detail at the default threshold
    y = sf.read(SRC / "exports/full_thr_default.wav")[0][:n_ramp, 0]
    m = (x > 0.5) & (x < 2.0)
    xi2 = np.linspace(0.5, 2.0, 800)
    fig, ax2 = plt.subplots(figsize=(8, 5))
    ax2.plot(x[m][::40], y[m][::40], ".", ms=4, alpha=0.5, label="measured")
    ax2.plot(xi2, curve_exp(xi2, 100 / 128), "r-", lw=1, label="exponential knee (formula)")
    ax2.plot(xi2, np.minimum(xi2, 1.0), "k--", lw=0.8, label="hard clip, for comparison")
    ax2.set_xlabel("Input")
    ax2.set_ylabel("Output")
    ax2.set_title("Knee detail, default threshold")
    ax2.legend()
    ax2.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(OUT / "clipper_02_knee_detail.png", dpi=DPI)
    plt.close(fig)

    # Plot 3: aliasing spectrum, 5 kHz sine at the '30%' threshold
    seg = sf.read(SRC / "exports/full_thr_low30.wav")[0][:, 0][int(31.3 * sr) : int(34.3 * sr)]
    w = np.hanning(len(seg))
    spec = 20 * np.log10(np.abs(np.fft.rfft(seg * w)) + 1e-12)
    spec -= spec.max()
    freqs = np.fft.rfftfreq(len(seg), 1 / sr)
    fig, ax3 = plt.subplots(figsize=(10, 5))
    ax3.plot(freqs / 1000, spec, lw=0.5)
    # Colour is carried per entry rather than sniffed from the label text,
    # so renaming a label can never silently flip a marker's colour.
    marks = [
        (5.0, "f0", "tab:green"),
        (15.0, "3rd harmonic", "tab:green"),
        (19.1, "alias of 5th (25k)", "tab:red"),
        (9.1, "alias of 7th (35k)", "tab:red"),
        (0.9, "alias of 9th (45k)", "tab:red"),
    ]
    for f, lbl, color in marks:
        i = np.argmin(np.abs(freqs - f * 1000))
        peak = spec[max(i - 30, 0) : i + 30].max()
        ax3.annotate(
            f"{lbl}\n{peak:+.0f} dB",
            (f, peak),
            textcoords="offset points",
            xytext=(0, 8),
            ha="center",
            fontsize=8,
            color=color,
        )
    ax3.set_ylim(-100, 10)
    ax3.set_xlabel("Frequency [kHz]")
    ax3.set_ylabel("dB rel. f0")
    ax3.set_title(
        "5 kHz sine @ +11 dB drive, threshold 67/128: harmonics (green) vs aliases (red)"
    )
    ax3.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(OUT / "clipper_03_aliasing_5k.png", dpi=DPI)
    plt.close(fig)

    print("wrote plots to", OUT)


if __name__ == "__main__":
    main()
