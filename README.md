# Lunar Lander for Flipper Zero

Clauded 'from-scratch,' inspired by 1979 Atari arcade game.

## Status
- [x] Menu screen — thrust-mode picker, default focus on selector, in-line descriptions
- [x] Game screen — physics, procedural terrain (seeded per level), rotated lander, thrust flame scaled by current thrust, HUD, crash/landing detection, level advance / retry
- [x] Terrain quality — normalized to span full vertical range; spike removal (peaks <3 px wide and >7 px tall are flattened)
- [x] Lander starts centered with random ±5 px/sec horizontal velocity (deterministic per level seed)
- [x] Level number shown at top center of HUD ("L1" through "L10")
- [x] Scoring on successful landing: `fuel_left × pad_multiplier × (HIGHEST_LEVEL − level + 1)`
- [ ] Per-pad varied multipliers (all currently default to 2x)
- [ ] Tutorial screen (placeholder)
- [ ] Sound
- [ ] Persisted high score
- [ ] "Game complete" screen when finishing HIGHEST_LEVEL (currently wraps to level 1)
- [ ] Tuning pass on physics constants

## Build & install

```sh
pip install --upgrade ufbt    # one time
ufbt                          # build .fap into dist/
ufbt launch                   # build, upload, and run on the connected Flipper
```

Make sure qFlipper and lab.flipper.net aren't connected to the Flipper while
running `ufbt launch` — they hold the serial port.

## Controls (in-game)
- Left / Right: rotate lander (hold for continuous rotation)
- Up: thrust (behavior depends on selected mode)
  - Binary: hold = full thrust
  - Tap Impulse: each press = fixed velocity kick along lander up-axis
  - Ramp: hold; thrust ramps from 0 to full over ~0.9 sec
- OK on land/crash banner: next level / retry
- Back: return to menu

## Pads per level
| Level | Pads |
|------:|-----:|
| 1–3   | 5    |
| 4–6   | 4    |
| 7–8   | 3    |
| 9     | 2    |
| 10    | 1    |

All pads are currently 16 px wide (2× lander). Scoring multipliers aren't
implemented yet — every pad just shows "2x" as a placeholder.

## Project layout
- `lunar_lander.c` — main loop, event dispatch, 60 Hz tick timer
- `lunar_lander.h` — shared types (ThrustMode, Screen)
- `menu.c` / `menu.h` — title screen
- `game.c` / `game.h` — physics, terrain, drawing
- `application.fam` — Flipper app manifest

## Tunables (in `game.c`)
The top of `game.c` has a block of `#define`s for gravity, thrust, rotation
rate, safe-landing thresholds, etc. They are pure guesses for feel — expect
to need a tuning pass.

## Known issues / TODO
- No screen flash or explosion animation on crash
- No animation/feedback for tap-impulse firing
- Lander rotation uses live `sinf/cosf` — fine for one lander at 60 Hz, but a
  16-step lookup table would be cheaper if we ever add more entities
- Score stays at 0 because the scoring system isn't wired up
- "Out of fuel" status exists in the enum but the game doesn't trigger it yet
  (collision handles all end states for now)
