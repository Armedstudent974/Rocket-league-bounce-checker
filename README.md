# Rocket League Bounce Checker

Shows a **red line** for where the ball is going and a **red box** on every
spot it will bounce, so you can read bounces and get to the ball in time.

- Red line: the predicted path of the ball's centre (3 s ahead by default)
- Red boxes: each wall, floor or ceiling bounce. The next bounce has the thickest outline
- Timer above each box: seconds until the ball gets there

> **Training only.** The overlay runs in **Freeplay, Custom Training and offline
> matches**. It turns itself off in online games. BakkesMod doesn't give plugins
> ball data there, and overlays in online play break Psyonix's rules.

## Install & run

1. Install [BakkesMod](https://bakkesmod.com/) (free, the standard Rocket League mod loader).
2. Download `RLBounceChecker.zip` from the latest **Actions → Build** run (artifacts) and unzip it.
3. Keep `RLBounceLauncher.exe` and `BounceChecker.dll` in the same folder and double-click
   `RLBounceLauncher.exe`. It:
   - copies the plugin into `%APPDATA%\bakkesmod\bakkesmod\plugins`
   - adds `plugin load bouncechecker` to BakkesMod's `plugins.cfg` (auto-load)
   - starts BakkesMod
   - launches Rocket League (Steam, or Epic if Steam isn't installed)
4. In game, go to **Freeplay**. The red line and boxes show up right away.

Launcher options: `--steam`, `--epic`, `--install-only`.

## In-game settings

Open the BakkesMod console with **F6** and type these:

| Command / cvar         | Default | What it does                               |
|------------------------|---------|--------------------------------------------|
| `bounce_toggle`        |         | Turn the overlay on/off (`bind F3 bounce_toggle`) |
| `bounce_enabled`       | 1       | Overlay on/off                              |
| `bounce_seconds`       | 3.0     | How far ahead to predict (0.5–8 s)          |
| `bounce_max_bounces`   | 3       | How many bounce boxes to draw               |
| `bounce_line_width`    | 3.0     | Line thickness                              |
| `bounce_show_timer`    | 1       | Show time-to-bounce labels                  |

## How it works

`plugin/src/BallPredictor.h` simulates the ball at 120 Hz using Rocket League's
physics: gravity −650 uu/s², drag, a 6000 uu/s speed cap, and the
community-measured bounce model (restitution 0.6, friction 0.285, spin
coupling). It also models the arena's floor, ceiling, walls, 45° corners and
goals. The curved wall ramps are treated as flat, so the prediction is slightly
off for balls rolling up a wall. It doesn't predict cars touching the ball,
since nobody can.

## Building from source

The GitHub Actions workflow (`.github/workflows/build.yml`) builds everything on
`windows-latest`. To build locally on Windows with Visual Studio 2022:

```powershell
git clone https://github.com/bakkesmodorg/BakkesModSDK.git ..\BakkesModSDK
cmake -S plugin   -B build/plugin   -A x64 -DBAKKESMOD_SDK=..\BakkesModSDK
cmake --build build/plugin --config Release
cmake -S launcher -B build/launcher -A x64
cmake --build build/launcher --config Release
```

Physics tests (any OS): `g++ -std=c++17 -Iplugin/src tests/predictor_test.cpp -o t && ./t`
