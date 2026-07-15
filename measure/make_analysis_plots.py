"""Plots zur Fruity-Soft-Clipper-Messung: Transferkurven + Fit, Aliasing-Spektrum."""

from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import soundfile as sf

BASE = Path(__file__).parent
OUT = BASE / "analysis"
OUT.mkdir(exist_ok=True)

def curve_exp(x, t):
    ax = np.abs(x)
    return np.sign(x) * np.where(ax <= t, ax, 1 - (1 - t) * np.exp(-(ax - t) / (1 - t)))


def main() -> None:
    dry, sr = sf.read(BASE / "exports/full_dry.wav")
    dry = dry[:, 0]
    n_ramp = int(8.0 * sr)
    x = dry[:n_ramp]

    # Plot 1: Transferkurven, Messung als Punkte, Formel als Linie
    fig, ax1 = plt.subplots(figsize=(8, 6))
    cases = [
        ("full_thr_high", 127 / 128, "max (127/128)", "tab:red"),
        ("full_thr_default", 100 / 128, "default (100/128)", "tab:blue"),
        ("full_thr_low30", 67 / 128, "'30%' (67/128)", "tab:green"),
    ]
    xi = np.linspace(-3.6, 3.6, 2000)
    for name, t, label, color in cases:
        y = sf.read(BASE / f"exports/{name}.wav")[0][:, 0][:n_ramp]
        ax1.plot(x[::400], y[::400], ".", ms=3, color=color, alpha=0.5)
        ax1.plot(xi, curve_exp(xi, t), "-", lw=1.2, color=color, label=f"Threshold {label}")
    ax1.axhline(1.0, color="gray", lw=0.5, ls="--")
    ax1.axhline(-1.0, color="gray", lw=0.5, ls="--")
    ax1.set_xlabel("Input")
    ax1.set_ylabel("Output")
    ax1.set_title("Fruity Soft Clipper Transferkurve\nPunkte = Messung, Linien = y = sign(x)(1-(1-t)exp(-(|x|-t)/(1-t)))")
    ax1.legend()
    ax1.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(OUT / "01_transferkurven.png", dpi=150)

    # Plot 2: Zoom aufs Knie (default)
    fig, ax2 = plt.subplots(figsize=(8, 5))
    y = sf.read(BASE / "exports/full_thr_default.wav")[0][:, 0][:n_ramp]
    m = (x > 0.5) & (x < 2.0)
    ax2.plot(x[m][::40], y[m][::40], ".", ms=4, alpha=0.5, label="Messung")
    xi2 = np.linspace(0.5, 2.0, 500)
    ax2.plot(xi2, curve_exp(xi2, 100 / 128), "r-", lw=1, label="Exponential-Knie")
    ax2.plot(xi2, np.minimum(xi2, 1.0), "k--", lw=0.8, label="Hardclip zum Vergleich")
    ax2.set_xlabel("Input")
    ax2.set_ylabel("Output")
    ax2.set_title("Knie-Detail, Threshold default")
    ax2.legend()
    ax2.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(OUT / "02_knie_detail.png", dpi=150)

    # Plot 3: Aliasing-Spektrum 5-kHz-Sinus, low30
    seg = sf.read(BASE / "exports/full_thr_low30.wav")[0][:, 0][int(31.3 * sr) : int(34.3 * sr)]
    w = np.hanning(len(seg))
    spec = 20 * np.log10(np.abs(np.fft.rfft(seg * w)) + 1e-12)
    spec -= spec.max()
    freqs = np.fft.rfftfreq(len(seg), 1 / sr)
    fig, ax3 = plt.subplots(figsize=(10, 5))
    ax3.plot(freqs / 1000, spec, lw=0.5)
    for f, lbl in [(5, "f0"), (15, "3. Harm."), (19.1, "Alias 25k"), (9.1, "Alias 35k"), (0.9, "Alias 45k")]:
        i = np.argmin(np.abs(freqs - f * 1000))
        peak = spec[max(i - 30, 0) : i + 30].max()
        color = "tab:green" if "Harm" in lbl or lbl == "f0" else "tab:red"
        ax3.annotate(f"{lbl}\n{peak:+.0f} dB", (f, peak), textcoords="offset points",
                     xytext=(0, 8), ha="center", fontsize=8, color=color)
    ax3.set_ylim(-100, 10)
    ax3.set_xlabel("Frequenz [kHz]")
    ax3.set_ylabel("dB rel. f0")
    ax3.set_title("5-kHz-Sinus @ +11 dB Drive, Threshold 67/128: Harmonische (gruen) vs. Aliasing (rot)")
    ax3.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(OUT / "03_aliasing_5k.png", dpi=150)
    print("Plots geschrieben nach", OUT)


if __name__ == "__main__":
    main()
