# TagoClip 1.1.1

Fixes a cut-off interface on hosts that apply display scaling, and makes the
plugin window resizable.

## Fixed

- **Interface cut off in some hosts.** On hosts that hand the plugin a window
  smaller than the interface, most often because of Windows or macOS display
  scaling above 100%, the right column and the bottom edge were cut off. The
  interface now scales into whatever window the host gives it. Reported against
  TagoPitch on Cakewalk Sonar and Fender Studio Pro; TagoClip had the same
  fixed-size interface.

## Added

- **Resizable window.** Drag the plugin window to any size you like. The aspect
  ratio stays locked, and the size is saved with the project.

## Install

- **macOS 11+:** open the `.dmg`, run the `.pkg` inside and pick the formats you
  want (AU + VST3, notarized).
- **Windows:** unzip and drop the `.vst3` into your VST3 folder.

Settings and presets from 1.1.0 carry over unchanged.
