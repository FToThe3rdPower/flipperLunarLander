# Lunar Lander for Flipper Zero

Clauded 'from-scratch,' tweaked by FToThe3rdPower, inspired by the 1979 Atari game.

## Requirements

- Any Flipper Zero. Developed on firmware 1.4.3; also builds against 1.5.1-rc.
- The Vidya tilt modes need the Flipper Zero Video Game Module.

## Status
- [x] Menu — thrust mode & fuel mode selectors, clickable title opens Info, lander icon opens High Score, wrench opens Settings
- [x] 30-level campaign with procedural terrain seeded per level; spike removal pass
- [x] Seeds 1–9999, each its own set of 30 levels (seed 1 is the original set)
- [x] Lander physics — gravity, thrust, rotation, wrapping, collision
- [x] Three button thrust modes: Button Tap Impulse (press), Button Binary (hold), Button Ramp (hold, ramps up)
- [x] Thrust button: UP or OK, chosen in Settings
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
- [x] Settings screen — Sound, Vibration, Difficulty, Thrust button, Seed, TV mode, HDMI audio out, Debug HUD, with a scrollbar; settings and menu choices are saved to the SD card
- [x] App icon
- [x] Persisted high score with the seed it was set on (saved to SD card; open it from the lander icon on the menu)
- [x] "Game complete" screen after level 30
- [x] High-multiplier pads are narrower: 3×→13 px, 5×→10 px
- [x] Multiplier labels drawn above a pad hide while the lander is over that pad
- [x] Pause menu — tap Back mid-flight: Resume, Zero tilt (tilt modes), Quit to menu; hold Back 1 s to leave
- [x] TV mode — squishes the playfield vertically for the Video Game Module's HDMI output
- [x] HDMI audio out (Auto / Yes / No) — sounds on the TV with the VGM480 module firmware
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
The thrust button is Up; Settings → Thrust button switches it to OK.

| Input | Action |
|-------|--------|
| Left / Right (hold) | Rotate lander |
| Thrust button — Button Binary | Hold = full thrust |
| Thrust button — Button Tap Impulse | Each press = fixed velocity kick |
| Thrust button — Button Ramp | Hold; thrust ramps 0→100% over ~0.5 s |
| OK on banner | Next level (landed) / Retry (crashed) |
| Back (tap) while flying | Pause menu |
| Back (hold 1 s) while flying or paused | Return to menu |
| Back on banner or popup | Return to menu |

### Pause menu
Tapping Back mid-flight freezes the game and opens the pause menu. Up and
Down choose, OK picks, and another Back tap resumes. **Zero tilt** (tilt
modes only) makes the way you're holding the Flipper the new "straight up"
for steering, then resumes. **Quit to menu** keeps a new high score, like
holding Back does.

### Vidya (VGM tilt) modes
Requires the Flipper Zero Video Game Module. Tilt left/right steers the lander
(device roll → lander angle, 1:1 mapping). Calibration captures the "upright"
position at the start of each level, including after retries. If the zero
drifts mid-flight, tap Back, hold the Flipper upright and pick Zero tilt.

| Mode | Thrust |
|------|--------|
| Vidya Tilt + Tap | The thrust button fires a burst |
| Vidya Tilt + Binary | Hold the thrust button |
| Vidya Tilt + Ramp | Hold the thrust button, ramps up |
| Vidya Full Tilt | Tilt forward = proportional thrust; no button needed |

## Difficulty

| Difficulty | Vy limit | Vx limit | Angle limit |
|------------|----------|----------|-------------|
| Easy       | < 16     | < 8      | < 25°       |
| Medium     | < 8      | < 4      | < 13°       |
| Hard       | < 4      | < 2      | < 6°        |
| Realistic  | < 1      | < 1      | < 3°        |
| Custom     | < 1–16   | < 1–8    | < 1–25°     |

For Custom, scroll the Difficulty row in Settings to Custom, then press OK to
set each limit (it can't be looser than Easy).

Only a pad's own width is flattened, so any flat ground is a pad (3× and 5×
pads are narrower). Touching down off a pad crashes, and the banner says
"Missed the pad".

## Seeds
Settings → Seed picks the world. Each seed from 1 to 9999 generates its own
30 levels (terrain, pads and starting drift); seed 1 is the original set.
Left/Right steps through seeds and OK picks a random one. The high score
screen shows the seed the high score was set on.

## TV mode
The Video Game Module always outputs 4:3 and shows each Flipper pixel 2 wide
by 3 tall, so the game looks stretched on a TV. Settings → TV mode draws the
playfield (terrain, lander, pads) at 2/3 height to compensate; physics is
unchanged. Text isn't squished.

The module's own firmware sets that picture, and an app can't change it: the
Flipper only streams its 128×64 frame. The firmware
([video-game-module](https://github.com/flipperdevices/video-game-module),
`app/frame.c`) always sends 640×480 at 60 Hz, the one mode every HDMI display
must accept, and repeats each Flipper row on 3 lines to fill more of the
screen.

The **VGM480** firmware for the module (below) sends 720×480 flagged as 16:9
instead, with square pixels that fill most of a widescreen TV. Turn TV mode
off with it, or the picture is squished twice.

## HDMI audio out (VGM480)

The Video Game Module only receives the Flipper's screen, so the stock module
can't play the game's sounds. VGM480 is a modified module firmware
(`vgm480` branch of a local clone of
[video-game-module](https://github.com/flipperdevices/video-game-module);
install `dist/vgm480-1.0.uf2` with the Video Game Module Tool's "Install
Firmware from File") that reads a tone hidden in the screen and plays it over
HDMI.

Settings → HDMI audio out:

| Setting | Sound |
|---------|-------|
| Auto (default) | On the TV while the module reports an HDMI TV with sound, otherwise the Flipper's speaker |
| Yes | Always on the TV; silent if no TV is connected |
| No | Always the Flipper's speaker, as before |

How it works: the bottom row of the game screen is always ground. While the
TV is playing the sound, two pixels at the right end of that row are flipped
to encode the current tone (20 Hz steps; `vgm_tone_channel.h`, shared with the
firmware). The ground under those pixels is always at least 2 px tall, or a
pad on the bottom row, so they look like a notch in the ground; pad labels
stay off the bottom row. The module answers by writing
`apps_data/vgm480/status.txt` (`audio=1` while it can play sound); the game
deletes that file when it starts and checks it once a second on Auto. The TV
lags the Flipper by about one frame.

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
Turn it on in Settings. It replaces the normal HUD while flying (the pause
menu and the landed/crashed banner hide it):

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
| `pause_menu.c / .h` | Pause menu rows and drawing; `game.c` decides what each choice does |
| `vgm_tone_channel.h` | Encodes the current tone into the screen's bottom row for the VGM480 module firmware (an identical copy lives in the firmware) |
| `game.c / .h` | Physics, terrain, collision, audio, drawing |
| `lander_sprite.c / .h` | Lander silhouette — static and rotated with flame |
| `vgm_tilt.c / .h` | VGM IMU wrapper (pitch/roll → steer/thrust) |
| `sensors/` | ICM-42688P driver and IMU fusion (VGM tilt hardware layer), from [flipperzero-game-engine](https://github.com/flipperdevices/flipperzero-game-engine) (GPL-3.0) |
| `application.fam` | Flipper app manifest (name, version, short description, icon) |
| `docs/description.md` | App description shown in the Flipper Apps Catalog (limited Markdown) |
| `changelog.md` | Version history, also shown in the catalog |

## Tunables
The top of `game.c` has a `#define` block for gravity, thrust, rotation rate,
the tilt-thrust dead-zone, and audio frequencies. The safe-landing thresholds
for each difficulty are set in `apply_difficulty()` in `game.c`.
