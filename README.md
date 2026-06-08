# Lunar Lander for Flipper Zero

Clauded 'from-scratch,' tweaked by FToThe3rdPower, inspired by the 1979 Atari game.

## Status
- [x] Menu — thrust mode & fuel mode selectors, clickable title opens Info, wrench opens Settings
- [x] 10-level campaign with procedural terrain seeded per level; spike removal pass
- [x] Lander physics — gravity, thrust, rotation, wrapping, collision
- [x] Three button thrust modes: Binary (hold UP), Tap Impulse (press UP), Ramp (hold UP, ramps up)
- [x] Four Video Game Module "vidya" tilt control modes.
- [x] Fuel campaign modes: Full refuel, or No-refuel (Easy 500 / Med 350 / Hard 200 total)
- [x] Difficulty selector — scales safe-landing thresholds (Easy / Medium / Hard / Realistic)
- [x] Distance-based pad multipliers: 1× (right below spawn), 2×, 3×, 5× (near edges)
- [x] Scoring: `fuel × pad_multiplier × (HIGHEST_LEVEL − level + 1)`
- [x] Level number shown at top center of HUD (L1–L10; T1–T2 during tutorial)
- [x] HUD shows score, time, fuel (left) and angle θ, Vx, Vy (right)
- [x] Sound — thrust tone (pitch scales with thrust level), tap blip, landing chime, crash rumble
- [x] Vibration — continuous on thrust, 3-pulse celebration on landing, rumble on crash
- [x] Crash/landing banner — shows Vx and Vy at touchdown; bad values blink on crash
- [x] 20% dim overlay behind status banners
- [x] Tutorial — 2 levels: flat terrain, full-width pad, ½ gravity (level 1) → full gravity, two pads at ⅓ and ⅔ width (level 2)
- [x] Tutorial intro and transition popups adapt to the selected thrust mode and difficulty
- [x] Settings screen — Sound, Vibration, Difficulty
- [x] App icon
- [x] Persisted high score (saved to SD card, shown on menu)
- [x] "Game complete" screen after level 10
- [x] High-multiplier pads are narrower: 3×→12 px, 5×→8 px

## Build & install
### Build from source
```sh
pip install --upgrade ufbt    # one time
ufbt                          # builds .fap into dist/
ufbt launch                   # build, upload, and run on the connected Flipper
```

Make sure qFlipper and lab.flipper.net aren't holding the serial port when
running `ufbt launch`.

### Run without building
Transfer `dist/lunar_lander.fap` directly to your Flipper and run it from the
Apps menu.

## Controls

### Button modes
| Input | Action |
|-------|--------|
| Left / Right (hold) | Rotate lander |
| Up — Binary | Hold = full thrust |
| Up — Tap Impulse | Each press = fixed velocity kick |
| Up — Ramp | Hold; thrust ramps 0→100% over ~0.7 s |
| OK on banner | Next level (landed) / Retry (crashed) |
| Back | Return to menu |

### Vidya (VGM tilt) modes
Requires the Flipper Zero Video Game Module. Tilt left/right steers the lander
(device roll → lander angle, 1:1 mapping). Calibration captures the "upright"
position at the start of each level, including after retries.

| Mode | Thrust |
|------|--------|
| Vidya Tilt + Tap | UP fires a burst |
| Vidya Tilt + Binary | Hold UP |
| Vidya Tilt + Ramp | Hold UP, ramps up |
| Vidya Full Tilt | Tilt forward = proportional thrust; no button needed |

## Difficulty

| Difficulty | Vy limit | Vx limit | Angle limit |
|------------|----------|----------|-------------|
| Easy       | < 16     | < 8      | < 25°       |
| Medium     | < 8      | < 4      | < 13°       |
| Hard       | < 4      | < 2      | < 6°        |
| Realistic  | < 1      | < 1      | < 3°        |

## Pads per level

| Level | Pads |
|------:|-----:|
| 1–3   | 5    |
| 4–6   | 4    |
| 7–8   | 3    |
| 9     | 2    |
| 10    | 1    |

Pad multipliers are distance-based — 1× directly below spawn, 2× nearby,
3× mid-range, 5× near screen edges. Pad widths match difficulty: 1× and 2×
pads are 16 px wide; 3× pads are 12 px; 5× pads are 8 px (just wider than
the lander's footprint).

## Project layout
| File | Purpose |
|------|---------|
| `lunar_lander.c` | Main loop, event dispatch, 60 Hz tick, screen transitions |
| `lunar_lander.h` | Shared enums — ThrustMode, FuelMode, Difficulty, Screen |
| `menu.c / .h` | Title/menu screen and mode selectors |
| `game.c / .h` | Physics, terrain, collision, audio, drawing |
| `lander_sprite.c / .h` | Lander silhouette — static and rotated with flame |
| `vgm_tilt.c / .h` | VGM IMU wrapper (pitch/roll → steer/thrust) |
| `sensors/` | ICM-42688P driver (VGM tilt hardware layer) |
| `application.fam` | Flipper app manifest |

## Tunables
The top of `game.c` has a `#define` block for gravity, thrust, rotation rate,
safe-landing thresholds, tilt dead-zones, and audio frequencies. The top of
`lunar_lander.h` defines the `Difficulty` thresholds used at runtime.
