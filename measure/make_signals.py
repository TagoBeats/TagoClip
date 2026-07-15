"""Generiert Testsignale zum Vermessen des Fruity Soft Clipper.

Alle Files: 44.1 kHz, 32-bit float WAV, mono. Pegel teils ueber 0 dBFS,
das ist Absicht (Float-Headroom), damit wir die Transferkurve auch
oberhalb des Clip-Punkts sehen.
"""

import argparse
from pathlib import Path

import numpy as np
import soundfile as sf

SR = 44100


def save(name: str, x: np.ndarray, outdir: Path) -> None:
    path = outdir / name
    sf.write(path, x.astype(np.float32), SR, subtype="FLOAT")
    print(f"{path.name}  ({len(x) / SR:.1f} s, peak {np.max(np.abs(x)):.2f})")


def triangle_ramp(amp: float, seconds: float) -> np.ndarray:
    """Langsames Dreieck ueber beide Polaritaeten, fuer die Transferkurve."""
    n = int(SR * seconds)
    t = np.linspace(0.0, 1.0, n, endpoint=False)
    cycles = 4
    phase = (t * cycles) % 1.0
    tri = 4.0 * np.abs(phase - 0.5) - 1.0
    return amp * tri


def sine(freq: float, amp: float, seconds: float) -> np.ndarray:
    n = int(SR * seconds)
    t = np.arange(n) / SR
    x = amp * np.sin(2.0 * np.pi * freq * t)
    fade = int(0.01 * SR)
    env = np.ones(n)
    env[:fade] = np.linspace(0.0, 1.0, fade)
    env[-fade:] = np.linspace(1.0, 0.0, fade)
    return x * env


def transient_train(amp: float, seconds: float) -> np.ndarray:
    """Kurze exponentiell abfallende Bursts alle 250 ms, wie Drum-Hits."""
    n = int(SR * seconds)
    x = np.zeros(n)
    burst_len = int(0.02 * SR)
    t = np.arange(burst_len) / SR
    burst = np.exp(-t * 300.0) * np.sin(2.0 * np.pi * 1000.0 * t)
    step = int(0.25 * SR)
    for start in range(0, n - burst_len, step):
        x[start : start + burst_len] = amp * burst
    return x


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--outdir", type=Path, default=Path(__file__).parent / "signals")
    args = parser.parse_args()
    args.outdir.mkdir(parents=True, exist_ok=True)

    save("01_ramp_transfer.wav", triangle_ramp(amp=2.0, seconds=8.0), args.outdir)
    save("02_sine_1k_0dB.wav", sine(1000.0, 1.0, 4.0), args.outdir)
    save("03_sine_1k_+6dB.wav", sine(1000.0, 2.0, 4.0), args.outdir)
    save("04_sine_100Hz_+6dB.wav", sine(100.0, 2.0, 4.0), args.outdir)
    save("05_sine_5k_+6dB.wav", sine(5000.0, 2.0, 4.0), args.outdir)
    save("06_transients_+6dB.wav", transient_train(amp=2.0, seconds=4.0), args.outdir)


if __name__ == "__main__":
    main()
