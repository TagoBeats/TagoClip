"""Sanity-Check der FL-Bounces: Format, Ausrichtung, Dry-Identitaet, Clipper-Wirkung."""

from pathlib import Path

import numpy as np
import soundfile as sf

BASE = Path(__file__).parent
SIGNALS = sorted((BASE / "signals").glob("*.wav"))
EXPORTS = BASE / "exports"


def load_mono(path: Path):
    x, sr = sf.read(path, always_2d=True)
    info = sf.info(path)
    mono_ok = x.shape[1] == 1 or np.allclose(x[:, 0], x[:, -1])
    return x[:, 0], sr, info.subtype, x.shape[1], mono_ok


def find_offset(hay: np.ndarray, needle: np.ndarray, start: int) -> int:
    """Findet needle in hay ab start via Kreuzkorrelation auf einem Ausschnitt."""
    seg = needle[: 44100 // 2]
    window = hay[start : start + len(seg) + 44100 * 20]
    corr = np.correlate(window, seg, mode="valid")
    return start + int(np.argmax(np.abs(corr)))


def main() -> None:
    dry, sr, subtype, ch, mono_ok = load_mono(EXPORTS / "full_dry.wav")
    print(f"full_dry.wav: {sr} Hz, {subtype}, {ch} ch (L==R: {mono_ok}), {len(dry)/sr:.1f} s")

    renders = {}
    for name in ["full_thr_default", "full_thr_high", "full_thr_low30"]:
        x, sr2, st2, ch2, mono2 = load_mono(EXPORTS / f"{name}.wav")
        renders[name] = x
        flags = [] if (sr2 == sr and st2 == subtype and len(x) == len(dry)) else ["FORMAT-MISMATCH!"]
        if not mono2:
            flags.append("L!=R")
        print(f"{name}.wav: {sr2} Hz, {st2}, {len(x)/sr2:.1f} s {' '.join(flags)}")

    print("\nPro Signal: Offset im Bounce, Dry-Abweichung, Wirkung des Clippers")
    pos = 0
    offsets = {}
    for sig_path in SIGNALS:
        sig, _ = sf.read(sig_path)
        off = find_offset(dry, sig, pos)
        offsets[sig_path.name] = off
        seg = dry[off : off + len(sig)]
        err = np.max(np.abs(seg - sig)) if len(seg) == len(sig) else np.inf
        diffs = {n: np.max(np.abs(r[off : off + len(sig)] - sig)) for n, r in renders.items()}
        pos = off + len(sig)
        print(
            f"  {sig_path.name}: offset {off/sr:6.2f}s | dry-err {err:.2e} | "
            f"max-delta default/high/low: "
            + "/".join(f"{diffs[n]:.3f}" for n in renders)
        )

    np.save(EXPORTS / "offsets.npy", offsets, allow_pickle=True)
    print("\nOffsets gespeichert nach exports/offsets.npy")


if __name__ == "__main__":
    main()
