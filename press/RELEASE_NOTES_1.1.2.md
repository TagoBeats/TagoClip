# TagoClip 1.1.2

Fixes the preset name jumping back to INIT after you close and reopen the
plugin window.

## Fixed

- **Preset name reverted to INIT.** Pick a preset, close the plugin window,
  open it again, and the header said INIT. The sound and every knob were
  correct the whole time, only the name was wrong. The name now comes from the
  actual settings, and reads CUSTOM once you move a knob away from the preset.
  The arrows step on from whatever preset is shown. Thanks to everyone who
  reported this one.

## Changed

- **New interface font.** The numbers and labels now use Martian Mono.

## Install

- **macOS 11+:** open the `.dmg`, run the `.pkg` inside and pick the formats you
  want (AU + VST3, notarized).
- **Windows:** unzip and drop the `.vst3` into your VST3 folder.

Settings and presets from earlier versions carry over unchanged.
