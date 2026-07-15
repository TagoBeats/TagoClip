# Fruity Soft Clipper: Messergebnis (15.07.2026)

## Der Algorithmus, vollstaendig identifiziert

Reiner statischer Waveshaper, sample-weise, keinerlei Zustand:

```
t = threshold                        # intern Integer p/128, Knob-Default p=100
y = x                                fuer |x| <= t
y = sign(x) * (1 - (1-t) * exp(-(|x|-t) / (1-t)))   sonst
```

- Unity-Gain und exakt linear unterhalb t, Steigung am Knie stetig (C1)
- Ceiling asymptotisch 1.0 (0 dBFS), unabhaengig vom Threshold
- Threshold verschiebt nur den Kniepunkt: max = 127/128 (praktisch Hardclip),
  Default = 100/128 = 0.78125, gemessenes "30%"-Setting = 67/128
- Perfekt ungerade-symmetrisch (nur ungerade Harmonische)

## Beweisqualitaet

- Formel auf den kompletten 42-s-Bounce angewendet (Rampe, 4 Sinusse,
  Transienten-Train): max. Abweichung 4.7e-8 = Float32-Quantisierung.
  Das Plugin ist zu 100% diese Formel. Kein Oversampling, keine Glaettung,
  kein DC-Handling, nichts.
- Alternativ-Fits (tanh-Knie, atan-Knie) liegen 4-5 Groessenordnungen drueber.

## Aliasing (die Schwachstelle, unser Ansatzpunkt)

5-kHz-Sinus @ +11 dB Drive, Threshold 67/128 (Plot 03):

| Komponente | Pegel rel. f0 |
|---|---|
| 3. Harmonische (15 kHz, legitim) | -12 dB |
| Alias der 5. Harm. (25k -> 19.1 kHz) | -19 dB |
| Alias der 7. Harm. (35k -> 9.1 kHz) | -25 dB |
| Alias der 9. Harm. (45k -> 0.9 kHz!) | -32 dB |

Ein Alias-Ton bei 900 Hz mit -32 dB ist klar hoerbar und unharmonisch.
Bei 808s (Fall 100 Hz) ist es harmloser, bei allem Obertonreichen (Snares,
Hi-Hats, Synths) nicht.

## Konsequenz fuer TagoClip

1. "FL Mode": exakt diese Formel als Kurven-Modus, mit t-Parameter 1:1
2. Oversampling (4x-8x, polyphase) drumherum: gleicher Sound, ohne den
   Alias-Wald. Das ist der messbare, demonstrierbare USP gegenueber dem
   Original und der Kern der Case Study
3. Weitere Kurven (Hard, tanh) als Modi, gleiche Engine

## Files

- 01_transferkurven.png: Messung vs. Formel, alle drei Settings
- 02_knie_detail.png: Knie-Zoom, Default-Threshold
- 03_aliasing_5k.png: Spektrum mit markierten Harmonischen vs. Aliasen
- ../exports/settings.txt: Gain-Staging und Segment-Offsets der Messung
