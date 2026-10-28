# C4Doom

A Doom (1993) engine for Android, written from scratch in pure C.

It is a port of the Java project JVDoom. The engine has no Java or Kotlin code: the app is a single native library (`libc4doom.so`) running on `NativeActivity`.

## Known issues

The game may contain bugs. If you find one, please report it in the Issues section of this repository.

## Features

- Software BSP renderer with sprites, masked textures, sky and animated flats/textures
- Original game logic: monsters, weapons, pickups, doors, lifts, switches, teleporters, secrets
- Original status bar, menu and intermission screens, drawn from the WAD
- Touch controls
- Save and load (6 slots)
- Sound effects and music (MUS converted to MIDI)
- Setup menu: resolution, aspect ratio, field of view, brightness, color filters, crosshair, control size and opacity, game speed, damage multipliers, cheats

## Requirements

| Item | Version |
|---|---|
| Package name | `com.c.c4doom` |
| NDK | 29.0.14033849 |
| CMake | 3.31.6 |
| Android Gradle Plugin | 8.13.0 |
| minSdk / targetSdk / compileSdk | 21 / 34 / 36 |
| ABIs | arm64-v8a, armeabi-v7a, x86_64, x86 |

## Building

1. Open the project in Android Studio (or any Gradle-based Android IDE).
2. Make sure NDK 29.0.14033849 and CMake 3.31.6 are installed.
3. Put a WAD file into `app/src/main/assets/` (see below).
4. Run `./gradlew assembleDebug`.

The project has no library dependencies.

## WAD files

The game needs a Doom IWAD. It looks for one in this order:

1. `Android/data/com.c.c4doom/files/` on the device
2. The app's internal files directory
3. The APK assets (`app/src/main/assets/`)

Recognized names: `doom1.wad`, `doom.wad`, `doom2.wad`, `freedoom1.wad`, `freedoom2.wad`, `doomu.wad`, `plutonia.wad`, `tnt.wad`. If none of these is present, the first `.wad` file found is used.

The WAD must contain compiled BSP nodes. If no WAD is found, the app shows an error screen; copy a WAD to one of the locations above and tap the screen to retry.

The shareware `doom1.wad` is included in the assets for convenience. Replace it with your own file if needed.

## Controls

| Control | Action |
|---|---|
| Left side of the screen | Virtual stick: move and strafe |
| Right side of the screen (drag) | Turn |
| FIRE | Fire the current weapon (dragging on it also turns) |
| USE | Open doors, press switches |
| Arrow buttons | Previous / next weapon |
| MAP | Toggle automap |
| MENU | Open the menu |
| Back button | Open the menu / go back |

## Project structure

All source files are in `app/src/main/cpp/`.

| File | Purpose |
|---|---|
| `platform_android.c` | Window, input, WAD discovery, OpenSL ES audio, JNI calls (music, vibration, fullscreen) |
| `view.c` | Main loop, screen layout, touch controls, HUD, automap, overlays |
| `game.c`, `ai.c`, `phys.c`, `specials.c` | Simulation at 35 Hz: player, monsters, collision, line specials |
| `render.c` | Software renderer |
| `canvas.c` | 2D drawing: fills, circles, lines, scaling, text |
| `hud.c`, `menu.c`, `intermission.c`, `savegame.c` | Status bar, menus, level-end screen, saves |
| `level.c`, `wad.c`, `gfx.c`, `tables.c` | Map loading, WAD reading, graphics decoding, thing and monster tables |
| `sound.c`, `music.c`, `prefs.c` | Mixer, MUS to MIDI, settings file |
| `doom.h` | Shared types and prototypes |

JNI is used from C for three things only: music playback through `MediaPlayer`, vibration, and fullscreen mode.

## Differences from JVDoom

- There is no WAD file picker. The WAD is read from the locations listed above.
- Save files use a new format; saves from JVDoom cannot be loaded.
- Settings are stored in `c4doom.cfg` in the app's files directory.
- Control labels are drawn with the Doom font instead of a system font.
- The animated background on the intermission screen does not work. The lump name is built incorrectly, a bug inherited from JVDoom.

## Host test harness

`host/host_main.c` runs the engine on a desktop computer. It renders frames to PPM files, tests save/load, and plays through all maps with random input. It is not part of the Android build. See `host/README` for the build command.

## License

The source code of this project is released under the MIT License. See the `LICENSE` file. Copyright (c) 2026 CanavarIT.

The MIT License covers only the game logic and engine code. It does not cover the Doom shareware data (`doom1.wad`) or any other WAD file, which are the property of id Software.
