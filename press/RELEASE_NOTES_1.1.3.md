# TagoClip 1.1.3

A fix release. One sound fix for everyone, two Windows-only fixes.

## Fixed

- **Mono Low no longer phases against the Mix knob.** With Mono Low on and
  Mix below 100%, the dry signal was not phase aligned with the processed one,
  so the low mids around the Mono Low frequency cancelled out (fully at 50%
  mix). The dry side now goes through the same phase response, and the blend
  stays flat at every mix setting. With Mono Low off nothing changes, the
  output is bit-identical to 1.1.2.
- **Windows: DAW shortcuts work while the plugin window is open.** After a
  click in the interface, the window held on to the keyboard, so Space for
  play/stop and other shortcuts did not reach the DAW. The plugin now hands
  the keyboard back after every click.
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
