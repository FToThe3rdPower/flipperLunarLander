# Lunar Lander for Flipper Zero

A from-scratch port of the 1979 Atari arcade game.

## Status
- [x] Menu screen with thrust-mode picker (Binary / Tap Impulse / Ramp)
- [ ] Game screen (physics, terrain, HUD, landing detection)
- [ ] Tutorial screen
- [ ] Score persistence

## Build & install

You'll need Python 3.8+ and a USB cable that supports data.

```sh
# one-time
pip install --upgrade ufbt

# from this directory
ufbt              # builds dist/lunar_lander.fap
ufbt launch       # builds, uploads to your Flipper, and runs it
```

`ufbt launch` requires that qFlipper and lab.flipper.net are NOT running
(they hold the serial port).

## Firmware notes
Built against the latest official SDK by default. Should also build on
Momentum FW — the GUI/canvas/input APIs used here are the standard ones
shared across forks. If you hit an API mismatch, pin uFBT to your firmware
channel: `ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json`

## Controls (menu)
- D-pad Up/Down: switch between thrust selector and buttons
- D-pad Left/Right on selector: cycle thrust mode
- D-pad Left/Right on buttons: pick START / TUTORIAL
- OK: activate the selected button
- Back: exit
