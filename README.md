# Lunar Lander for Flipper Zero

Clauded 'from-scratch,' tweaked by FToThe3rdPower, inspired by the 1979 Atari game.

## Status
- [x] Menu — thrust mode & fuel mode selectors, clickable title opens Info, lander icon opens High Score, wrench opens Settings
- [x] 30-level campaign with procedural terrain seeded per level; spike removal pass
- [x] Lander physics — gravity, thrust, rotation, wrapping, collision
- [x] Three button thrust modes: Binary (hold UP), Tap Impulse (press UP), Ramp (hold UP, ramps up)
- [x] Four Video Game Module "vidya" tilt control modes.
- [x] Fuel campaign modes: Full refuel, or No-refuel (Easy 500 / Med 350 / Hard 200 total)
- [x] Difficulty selector — scales safe-landing thresholds (Easy / Medium / Hard / Realistic / Custom)
- [x] Distance-based pad multipliers: 1× (right below spawn), 2×, 3×, 5× (near edges)
- [x] Scoring: `fuel × pad_multiplier × (HIGHEST_LEVEL − level + 1)`
- [x] Level number shown at top center of HUD (L1–L30; T1–T2 during tutorial)
- [x] HUD shows score, time, fuel (left) and angle θ, Vx, Vy (right)
- [x] Sound — thrust tone (pitch scales with thrust level), tap blip, landing chime, crash rumble; Off / Low / Med / High
- [x] Vibration — continuous on thrust, pulse per tap, 3-pulse celebration on landing, rumble on crash; Off / Low / Med / High
- [x] Crash/landing banner — shows Vx and Vy at touchdown (plus the angle when it caused the crash); bad values blink on crash
- [x] 20% dim overlay behind status banners
- [x] Tutorial — 2 levels: flat terrain, full-width pad, ½ gravity (level 1) → full gravity, two pads at ⅓ and ⅔ width (level 2)
- [x] Tutorial intro and transition popups adapt to the selected thrust mode and difficulty
- [x] Settings screen — Sound, Vibration, Difficulty, TV squish, Debug HUD; settings and menu choices are saved to the SD card
- [x] App icon
- [x] Persisted high score (saved to SD card; open it from the lander icon on the menu)
- [x] "Game complete" screen after level 30
- [x] High-multiplier pads are narrower: 3×→13 px, 5×→10 px
- [x] Multiplier labels drawn above a pad hide while the lander is over that pad
- [x] Mid-flight, a tap of Back re-zeroes tilt steering; hold Back 3 s to leave
- [x] TV mode — squishes the playfield vertically for the Video Game Module's HDMI output
- [x] Debug HUD (see below)

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
| Up — Ramp | Hold; thrust ramps 0→100% over ~0.5 s |
| OK on banner | Next level (landed) / Retry (crashed) |
| Back (hold 3 s) while flying | Return to menu |
| Back on banner or popup | Return to menu |

### Vidya (VGM tilt) modes
Requires the Flipper Zero Video Game Module. Tilt left/right steers the lander
(device roll → lander angle, 1:1 mapping). Calibration captures the "upright"
position at the start of each level, including after retries. If the zero
drifts mid-flight, hold the Flipper upright and tap Back to re-zero it.

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
| Custom     | < 1–16   | < 1–8    | < 1–25°     |

For Custom, press OK on the Difficulty row in Settings and set each limit
(it can't be looser than Easy).

## TV mode
The Video Game Module shows each Flipper pixel 2 wide by 3 tall, so the game
looks stretched on a TV. Settings → TV squish compresses the playfield
(terrain, lander, pads) vertically to compensate: 67% looks right on a 4:3
picture, about 90% if your TV stretches the picture to 16:9. Text isn't
squished.

In-game frames are drawn at ~31 fps (~15 fps while a banner or popup is up);
physics still runs at 60 Hz. Each frame is sent to the TV over a serial link,
and drawing fewer of them leaves the Flipper time for input.

## Pads per level

| Level | Pads |
|------:|-----:|
| 1–3   | 5    |
| 4–6   | 4    |
| 7–8   | 3    |
| 9     | 2    |
| 10–30 | 1    |

Pad multipliers are distance-based — 1× directly below spawn, 2× nearby,
3× mid-range, 5× near screen edges. Pad widths match difficulty: 1× and 2×
pads are 16 px wide; 3× pads are 13 px; 5× pads are 10 px (a few px wider
than the lander's 7 px footprint).

## Debug HUD
Turn it on in Settings. It replaces the normal HUD while flying (the
landed/crashed banner hides it):

| Field | Meaning |
|-------|---------|
| `HP` | Free heap (bytes) |
| `TK` | Game ticks processed (resets when you return to the menu) |
| `Q` | Event queue depth (current / peak) |
| `DR` | Longest frame draw in the last second (ms) |
| `TG` | Longest gap between game ticks in the last second (ms); ~16 is normal |
| `A` `X` `Y` `S` | Angle (°), Vx, Vy, and speed |

With Debug HUD on, everything except `S` is also logged at 6 Hz under
`LunarDbg` (`ufbt cli`, then `log`).

## Project layout
| File | Purpose |
|------|---------|
| `lunar_lander.c` | Main loop, event dispatch, 60 Hz tick, screen transitions |
| `lunar_lander.h` | Shared enums — ThrustMode, FuelMode, Difficulty, Screen |
| `menu.c / .h` | Title/menu screen and mode selectors |
| `game.c / .h` | Physics, terrain, collision, audio, drawing |
| `lander_sprite.c / .h` | Lander silhouette — static and rotated with flame |
| `vgm_tilt.c / .h` | VGM IMU wrapper (pitch/roll → steer/thrust) |
| `sensors/` | ICM-42688P driver and IMU fusion (VGM tilt hardware layer), from [flipperzero-game-engine](https://github.com/flipperdevices/flipperzero-game-engine) (GPL-3.0) |
| `application.fam` | Flipper app manifest |

## Tunables
The top of `game.c` has a `#define` block for gravity, thrust, rotation rate,
the tilt-thrust dead-zone, and audio frequencies. The safe-landing thresholds
for each difficulty are set in `apply_difficulty()` in `game.c`.
