# TagoClip 1.1.3

A Windows-only hardening release. macOS builds are unchanged apart from the
version number.

## Fixed

- **Windows: the interface keeps its own data folder.** The plugin's
  interface (Microsoft Edge WebView2) used to store its browser data in the
  shared Windows temp folder, together with every other app that does the same.
  It now uses its own folder under `%LOCALAPPDATA%\TagoBeats\TagoClip`. This
  may fix a crash some hosts showed right when the plugin was inserted. If your
  DAW still closes when you add TagoClip, please reply to the download mail
  with your Windows version and DAW.

## Install

- **macOS 11+:** open the `.dmg`, run the `.pkg` inside and pick the formats you
  want (AU + VST3, notarized).
- **Windows:** unzip and drop the `.vst3` into your VST3 folder.

Settings and presets from earlier versions carry over unchanged.
