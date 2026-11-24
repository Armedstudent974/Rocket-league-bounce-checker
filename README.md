# Rocket League Bounce Checker

A training tool for Rocket League's Freeplay and Custom Training modes.

- **Bounce prediction overlay**: a red line shows where the ball is going, and a
  red box marks every spot it will bounce, with a countdown to each bounce
- **Unlimited boost**
- **Ball reset and teleport**: ball to centre, ball to you, or you to the ball
- **Custom ball launches**: pick position, speed, direction and angle, and save up to 5 presets
- **Mechanic drills**: musty flicks, air dribbles and ceiling shots, with a
  random-variance slider so each rep is a little different

> **Freeplay and Custom Training only.** The training tools don't run in online
> matches (ranked, casual or private), and the overlay switches off there too.
> BakkesMod blocks plugins in online games, and changing the ball or boost there
> would be cheating.

## Install & run

1. Install [BakkesMod](https://bakkesmod.com/) (free, the standard Rocket League mod loader).
2. Download `RLBounceChecker.zip` from the latest **Actions → Build** run (Artifacts) and unzip it.
3. Keep `RLBounceLauncher.exe` and `BounceChecker.dll` in the same folder and double-click
   `RLBounceLauncher.exe`. It:
   - copies the plugin into `%APPDATA%\bakkesmod\bakkesmod\plugins`
   - adds `plugin load bouncechecker` to BakkesMod's `plugins.cfg` (auto-load)
   - starts BakkesMod
   - launches Rocket League (Steam, or Epic if Steam isn't installed)
4. In game, go to **Freeplay** and press **F2 → Plugins → Bounce Checker** to open the menu.

Launcher options: `--steam`, `--epic`, `--install-only`.

## The menu (F2 → Plugins → Bounce Checker)

| Section | What's in it |
|---|---|
| Bounce prediction overlay | On/off, how far ahead to predict, number of bounce boxes, line width, timers |
| Unlimited boost | On/off |
| Ball reset & teleport | **Reset ball to centre** · **Ball to me** · **Me to ball** |
| Custom ball launch | Position, speed, direction and upward angle sliders, **LAUNCH** button, 5 save/load preset slots. "Relative to my car" means X is forward and Y is right |
| Mechanic drills | **Musty flick** (ball starts on your roof), **Air dribble** (ball popped up in front of you), **Ceiling shot** (you start driving up the nearest wall), **Repeat last drill**, random variance |

### Key binds

Every button is also a console command. Bind them to keys in the **F6** console,
for example `bind F3 bounce_repeat` or `bind XboxTypeS_DPad_Up bounce_reset_ball`.

| Command | Does |
|---|---|
| `bounce_toggle` | Overlay on/off |
| `bounce_boost_toggle` | Unlimited boost on/off |
| `bounce_reset_ball` | Ball to centre |
| `bounce_ball_to_car` | Ball in front of you |
| `bounce_car_to_ball` | You behind the ball |
| `bounce_launch` | Custom launch |
| `bounce_musty` | Musty flick drill |
| `bounce_air_dribble` | Air dribble drill |
| `bounce_ceiling` | Ceiling shot drill |
| `bounce_repeat` | Repeat last drill/launch |

All settings are also cvars (`bounce_seconds`, `bounce_unlimited_boost`,
`bounce_launch_speed`, `bounce_variance`, …) and are saved between sessions.

## Playing with friends

Ranked, casual and private matches all run on Psyonix's servers. Plugins can't change
anything there, and this one switches itself off. To train with friends, each person
can run Bounce Checker in their own Freeplay.

## How it works

`plugin/src/BallPredictor.h` simulates the ball at 120 Hz using Rocket League's
physics: gravity −650 uu/s², drag, a 6000 uu/s speed cap, and the
community-measured bounce model (restitution 0.6, friction 0.285, spin
coupling). It also models the arena's floor, ceiling, walls, 45° corners and
goals. The curved wall ramps are treated as flat, so the prediction is slightly
off for balls rolling up a wall. It doesn't predict cars touching the ball.

`plugin/src/Scenarios.h` works out where the car and ball start for each drill.
Ground drills use your car's spot when there's room ahead. Otherwise they move you
to a clear spot in your half.

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

Tests (any OS):

```
g++ -std=c++17 -Iplugin/src tests/predictor_test.cpp -o t1 && ./t1
g++ -std=c++17 -Iplugin/src tests/scenarios_test.cpp -o t2 && ./t2
```
