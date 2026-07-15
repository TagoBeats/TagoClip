# Fruity Soft Clipper vermessen

Ziel: exakte Transferkurve + Aliasing-Verhalten des Fruity Soft Clipper, als Referenz fuer den TagoClip-DSP.

## Signale (measure/signals/)

| File | Zweck |
|---|---|
| 01_ramp_transfer.wav | Langsames Dreieck bis +6 dBFS, liefert die Transferkurve |
| 02_sine_1k_0dB.wav | Sinus 1 kHz @ 0 dBFS, Harmonics/Aliasing bei moderatem Drive |
| 03_sine_1k_+6dB.wav | Sinus 1 kHz @ +6 dBFS, hart gefahren |
| 04_sine_100Hz_+6dB.wav | Sinus 100 Hz, Low-End-Verhalten (808-Fall) |
| 05_sine_5k_+6dB.wav | Sinus 5 kHz, hier zeigt sich Aliasing am deutlichsten |
| 06_transients_+6dB.wav | Burst-Train wie Drum-Hits, Check ob der Clipper rein statisch ist |

Die Files sind 32-bit float und gehen absichtlich ueber 0 dBFS. FL rechnet intern in Float, das bleibt erhalten solange du auch in 32-bit float renderst.

## Setup in FL Studio

1. Neues leeres Projekt, Sample Rate 44100 Hz (Audio Settings)
2. Alle 6 WAVs als Audio Clips in die Playlist ziehen, nacheinander mit etwas Abstand
3. WICHTIG, Gain-Staging: alles auf exakt 0 dB / 100%
   - Channel-Volume jedes Audio Clips (FL default ist NICHT 100%, Rechtsklick auf den Volume-Knob → auf Maximum bzw. 100% setzen)
   - Mixer-Track-Fader und Master-Fader auf 0 dB
   - Master muss leer sein (kein Limiter, nichts)
4. Fruity Soft Clipper als einziges Plugin auf den Mixer-Track der Clips

## Renders (nach measure/renders/)

Render-Settings: WAV, **32-bit float**, kein Dithering, Mode "Full song" bzw. Selection ueber alle Clips. Ein Render enthaelt dann alle 6 Signale nacheinander, das ist ok, wir schneiden in Python.

Diese Durchgaenge, Naming exakt so:

1. `full_dry.wav` — Clipper **bypassed** (Sanity-Check: muss bit-identisch zum Input sein, sonst stimmt Gain-Staging nicht)
2. `full_thr_default.wav` — Clipper mit Default-Settings (nichts anfassen)
3. `full_thr_high.wav` — Threshold voll auf (Maximum)
4. `full_thr_low.wav` — Threshold ca. 30-50%
5. Post-Gain-Knob dabei immer auf Default lassen

Notiere zu 2-4 die **exakten Knob-Werte** (Hint-Bar oben links zeigt den Wert beim Drehen) in einer kurzen Textnotiz `renders/settings.txt`. Ohne die Werte koennen wir die Kurven spaeter nicht den Settings zuordnen.

Danach alles in measure/renders/ legen und Bescheid sagen, dann kommt die Python-Auswertung (Transferkurve fitten, Aliasing-Spektren, Statik-Check).

## Nachmessung Post-Gain (15.07.)

Frage: ist der Post-Knob ein simpler linearer Gain, und wie mappt die Knob-Skala?
Gleiches Projekt-Setup wie oben (Gain-Staging unveraendert lassen!), Threshold auf Default lassen, nur Post drehen:

1. `full_post_low.wav` — Post auf ca. 50% (exakten Hint-Bar-Wert notieren)
2. `full_post_max.wav` — Post auf Maximum (Wert notieren)

Render wieder 32-bit float ohne Dithering, nach measure/exports/, Werte in settings.txt ergaenzen.
Zwei Punkte reichen: Default (unity, schon gemessen) + zwei weitere zeigen ob linear und wie skaliert.
