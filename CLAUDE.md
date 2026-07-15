# TagoClip – Arbeitsregeln für Claude

## Projektkontext

Softclipper für 808s/Drums, Plugin Nr. 2 der Tago-Linie (nach TagoPitch, das als
Template diente). Kern: der reverse-vermessene Fruity Soft Clipper als "FL Mode"
(Kurve exakt, siehe measure/analysis/REPORT.md) plus Oversampling gegen dessen
Aliasing, plus Hard/Tanh-Kurven, Delta-Listen und Mono-Low-End.
Python-Referenz: tagodsp (distortion/clipper.py, stereo/mono_low.py).
Projekt-Notiz im Vault: 02 Projekte/TagoClip.md.

## Repo-Struktur

- `plugin/` C++ (Processor + ClipEngine). Parameter-IDs zentral in `PluginProcessor.h`
  (`tagoclip::param`). Editor ist noch der generische JUCE-Editor, WebView-Port folgt.
- `plugin/dsp/` DSP-Module: Curves.h, Oversampler.h (+ generierte OversamplerTaps.h),
  MonoLow.h. Ports der tagodsp-Prototypen, Golden-Tests halten sie deckungsgleich.
- `mockup/` abgenommenes Design (15.07.2026). **Read-only, Source of Truth für die UI.**
- `measure/` Fruity-Soft-Clipper-Vermessung (Report, Testsignale, FL-Bounces). Read-only.
- `golden/` Golden-File-Tests: make_reference.py (tagodsp-Referenzen) + compare.py.
  refs/ und out/ sind generiert und nicht eingecheckt.
- `tools/render_cli.cpp` Offline-Render-CLI für die Golden-Tests.
- `scripts/` gen_filter_taps.py (scipy-Taps nach C++), run_golden.sh, Build-Helfer.
- `third_party/JUCE` Submodule, gepinnt auf 8.0.14. Nicht ungefragt bumpen.

## Build und Dev-Loop

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DTAGOCLIP_BUILD_TOOLS=ON
cmake --build build --target TagoClip_Standalone TagoClipRender   # oder _VST3 / _AU

# Golden-Tests (braucht das tagodsp-venv):
scripts/run_golden.sh build/TagoClipRender_artefacts/TagoClipRender
```

`auval -v aufx TgCl Tago` nach Processor-Änderungen (AU vorher nach
~/Library/Audio/Plug-Ins/Components kopieren).

## Harte Regeln

1. **Parameter-Contract:** Die IDs in `tagoclip::param` (`drive_db`, `threshold_steps`,
   `curve`, `oversampling`, `output_db`, `mono_low`, `delta`, `bypass`) sind der Vertrag
   mit Python-Referenz, Golden-Tests und später den WebView-Relays. Niemals umbenennen,
   Ranges/Defaults nur nach Absprache ändern.
2. **Mockup und measure/ nicht anfassen:** beides abgenommen bzw. Messdaten.
3. **DSP-Änderungen reviewt Robin selbst.** DSP-Code (Kurven, Oversampler, MonoLow,
   Signalkette, Latenz) klar von UI-Arbeit trennen, im Ergebnis explizit als
   "DSP, bitte reviewen" ausweisen. UI-Code darf vibe-coded bleiben.
4. **Python-First + Golden-Parity:** Verhaltensfragen entscheidet die tagodsp-Referenz.
   Jede DSP-Änderung muss die Golden-Tests bestehen; ändert sich das Verhalten gewollt,
   erst den Python-Prototyp anpassen, dann Referenzen neu rendern (`--fresh`).
   Klangliche Urteile fällt Robin per Hörprobe (/listen-pack), nicht Claude per Plot.
5. **Oversampler-Taps sind generiert:** OversamplerTaps.h nie von Hand editieren,
   immer über scripts/gen_filter_taps.py (identisches Filter wie scipy resample_poly).
6. **Audio-Thread-Disziplin:** kein Locking/Allokieren/Logging in `processBlock`.
   Meter-Daten per Atomics an die UI.
7. **Latenz ist konstant 20 Samples** in allen Modi (OS-Off-Pfad wird passend verzögert),
   damit der OS-Toggle nie die PDC verschiebt. Nicht ändern ohne Absprache.
8. **Keine em-dashes** in Code-Kommentaren, Commit-Messages und Docs. Kommentare auf
   Englisch, knapp, nur wo der Code es nicht selbst sagt.

## Verifikation

- DSP: scripts/run_golden.sh gruen (inkl. Block-Sweep), bei hoerbaren Aenderungen
  Delta-Bounces via /listen-pack, finale Abnahme durch Robins Ohr.
- Formate: warnungsfreier Build (VST3/AU/Standalone), `auval` gruen.
- DAW-Test in FL Studio macht Robin selbst (FL ist die Zielgruppe-DAW).
