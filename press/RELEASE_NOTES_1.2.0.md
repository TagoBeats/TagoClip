# TagoClip 1.2.0

Output now only trims the processed signal, the preset name is a menu, and Pro Tools (AAX) is supported on macOS.

## Changed

- **Output only trims the processed signal.** The Output knob used to sit after
  the Mix blend, so pulling it down to compensate for a hard Drive also pulled
  down the dry signal as soon as Mix was below 100%. Now the dry side stays at
  its original level and Output only trims what the clipper made. At Mix 100%
  nothing changes. Thanks to the user who asked for this. Projects saved with
  an earlier version keep the old behaviour so they sound exactly as before;
  new instances and newly saved projects use the new one.
- **Mono Low only touches the sides.** The low cut now acts on the side signal
  only, the mid passes untouched, so the centre of your mix (kick, 808, vocal)
  keeps its phase and level while the low end of the stereo width tightens up.

## New

- **Preset menu.** Click the preset name in the header to open the full list
  and pick any preset directly. The arrows still step through the list, and the
  name still shows CUSTOM as soon as you move a knob away from a preset.
- **Pro Tools (AAX) on macOS.** TagoClip now comes as an AAX plugin for Pro
  Tools, mono and stereo, signed for Pro Tools. Windows AAX follows later.

## Install

- **macOS 11+:** open the `.dmg`, run the `.pkg` inside and pick the formats you
  want (AU + VST3 + AAX, notarized).
- **Windows:** unzip and drop the `.vst3` into your VST3 folder.

Settings and presets from earlier versions carry over unchanged.
