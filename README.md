# Heart of The Alien Redux 2.0.1

[![shot1](res/thumb1.png)](res/shot1.png)
[![shot2](res/thumb2.png)](res/shot2.png)

A modern port of *Heart of the Alien: Out of This World Parts I and II*
(Sega CD, 1994), the sequel to Éric Chahi's *Another World*.

This repository is an unofficial, fan-made continuation of **Heart of The
Alien Redux**, the 2004 reimplementation by **Gil Megidish**, which was later
updated to SDL3 for the **2.0.0** release. The **2.0.1** work here adds a
high-resolution OpenGL renderer, adjustable pixel-art smoothing, user
configurable key bindings, a long list of bug fixes and a Windows release.
It was made by **Jorge Salgueiro** with the help of **opencode** and
**DeepSeek**.

> **No game data is included.** You must own the original Sega CD game and
> provide its ISO image (and optionally the CD music) yourself. Game data is
> copyrighted: the `Heart Of The Alien (U)` folder, ISO images and CD audio
> tracks are ignored by git and must never be committed to this repository.

---

## Why this fork exists

I am a long-time *Another World* fan. *Another World* is one of those games
that stays with you: the art, the silence, the way the story is told without a
single word. For years I had heard that *Heart of the Alien* continued the story
directly where *Another World* ended, with you playing Buddy, the alien friend
from the first game — but I never got to try it.

When I finally found the ISO, I went looking for a way to play it on a modern
PC. That search led me to Gil Megidish's **Heart of The Alien Redux**, and later
to the **2.0.0** SDL3 port. Both are wonderful pieces of work.

I faced some problems, so I started fixing and polishing. One thing led to another, and this 2.0.1
fork is the result: the same game and the same Redux engine, with a sharper
picture, a documented key binding file, and some crashes and glitches
removed. The porting, renderer, shader tuning, key binding system, test suite
and this README were written together with **opencode** (the coding agent) and
**DeepSeek** (the model behind it) as an AI pair-programmer. *Heart of the
Alien* itself remains the work of its original creators — see
[Credits](#credits).

## What our version adds

### Picture

- **HD renderer.** The 304×192 game framebuffer is uploaded as a texture and
  drawn at the native window resolution through SDL3 and OpenGL, instead of
  being pre-scaled into a blocky bitmap. Nearest-neighbor stays the fallback
  for machines without OpenGL.
- **Sharp pixel-art upscaling.** The Omniscale filter reconstructs diagonal
  edges and round shapes instead of turning them into stair steps.
- **Adjustable smoothing (`--hd-smooth`).** Full Omniscale looked too soft, so
  its output is now blended with the untouched texel. The default of **30**
  removes the diagonal "wobble" while keeping lines and text crisp; `0` is
  identical to nearest-neighbor and `100` is the original full filter.
- **Lightweight.** No external scaler library and no post-processing chain:
  two small GLSL shaders and one texture upload per frame.

### Controls and quality of life

- **User configurable keys.** A documented `keys.txt` is written on first
  start. Change any binding, list several keys per action, use modifiers such
  as `ctrl+s` or `alt+enter`, or unbind an action by leaving the right side
  empty. Load another file with `--keys`, validate one with `--check-keys`.
- **Sane toggles.** <kbd>F3</kbd>/<kbd>F</kbd> switch between sharp and smooth
  rendering, <kbd>F2</kbd> cycles the classic upscaler, <kbd>F12</kbd> saves a
  high-resolution screenshot (rendered offscreen, so it also works when the
  window is partly hidden).
- **Window and timing fixes.** The window sizes itself to fit the desktop, the
  GL renderer no longer stalls input and audio when the window is occluded or
  minimized, and vsync no longer freezes the game on some drivers.
- **`play.bat` launcher.** Double-click it to start from the ISO with the
  recommended settings; extra arguments are passed through to `alien.exe`.

### Under the hood

- **Bug fixes** across the animation player, LZSS backdrop decoder, sprite
  blitter, VM, sound mixer, ISO/file handling, renderer and screenshot path
  (see the [changelog](#changelog)).
- **Bounds checking** for corrupt or unexpected sprite, save and path data, so
  a bad file gives a warning instead of a crash.
- **Regression suite.** `tests/run-tests.ps1` runs 37 headless scenarios
  (startup, every room, every upscaler, HD and SDL renderers, key binding
  parsing, recorded playthroughs) and `tests/run-asan.sh` runs the same kind
  of checks under AddressSanitizer/UBSan on Linux.
- **Reproducible Windows build.** MinGW-w64 + CMake + SDL3, packaged as a
  flat zip with the two required DLLs.

## Requirements

- Windows 10/11 (x64) for the release zip, or Linux/other platforms built from
  source with SDL3 and SDL3_mixer.
- A copy of the Sega CD game: `Heart Of The Alien (U).iso` plus, if you want
  music, the matching MP3/OGG/WAV/Opus/FLAC tracks.
- OpenGL 2.1 or newer for the HD renderer (optional; the game falls back to
  the plain SDL renderer).

## Running

### The easy way (Windows release zip)

1. Unpack `hota-redux-2.0.1-win64.zip` into a folder of your choice.
2. Put your game data next to `alien.exe`, keeping the original names:

   ```
   alien.exe
   SDL3.dll
   SDL3_mixer.dll
   play.bat
   Heart Of The Alien (U)\Heart Of The Alien (U).iso
   Heart Of The Alien (U)\Heart Of The Alien (U) 02.mp3
   ...
   ```

3. Double-click **`play.bat`**.

`play.bat` changes to its own folder and runs:

```
alien.exe --iso --hd --hd-smooth 30
```

so it works no matter where you unpacked the zip, and any extra arguments are
passed to the game:

```
play.bat --fullscreen
play.bat --sharp
play.bat --iso "My Rip"
```

### From the command line

```
alien.exe --iso
```

With no arguments the game plays the introduction and then shows the code
entry screen. The default ISO prefix is `Heart Of The Alien (U)`, and the
default music prefix follows the ISO prefix. The level 1 password is `BDXF`.

`alien.exe --help` prints the full option list.

## Command-line flags

### Video

| Flag | What it does |
| --- | --- |
| `--hd` | HD rendering through OpenGL at the native window resolution. **Default.** |
| `--no-hd` | Use the plain SDL renderer (pre-scaled bitmap). Useful on old drivers. |
| `--sharp` | HD: draw with a nearest-neighbor shader instead of Omniscale. |
| `--hd-smooth` `n` | HD: Omniscale strength, `0`–`100`. Default `30`: `0` = sharp, `100` = full smoothing. |
| `--scale` `x` | Window scale factor, `0` = automatic (fits the desktop). Ignored in fullscreen. |
| `--upscale` `n` | Internal pixel-art upscaling `1`–`4` for the classic renderer, `0` = automatic. |
| `--smooth` | Classic renderer: use Scale2x/Scale3x smoothing instead of sharp scaling. |
| `--filter` | Classic renderer: use bilinear hardware filtering. |
| `--fullscreen` | Start in fullscreen (also `alt+enter` at runtime). |

### Game data

| Flag | What it does |
| --- | --- |
| `--iso` [`prefix`] | Read game data from `<prefix>.iso`. The prefix is optional and defaults to `Heart Of The Alien (U)`. |
| `--music` `prefix` | Use this prefix for CD music (`.wav`, `.mp3`, `.ogg`, `.opus`, `.flac`). Defaults to the ISO prefix. |
| `--room` `n` | Start from script/room `n` instead of the normal game flow. |

### Input and capture

| Flag | What it does |
| --- | --- |
| `--keys` `file` | Read key bindings from `file` instead of `keys.txt`. |
| `--check-keys` | Parse the key bindings file, print warnings, and exit without starting the game. |
| `--record` | Record your key presses to `recorded-keys`. |
| `--replay` | Replay a previously recorded `recorded-keys` file. |

### Miscellaneous

| Flag | What it does |
| --- | --- |
| `--fastest` | Disable the speed throttle (mainly for tests and recording playback). |
| `--help` | Print the option list and exit. |

### Developer and test flags

| Flag | What it does |
| --- | --- |
| `--debug` | Verbose debug output (debug builds). |
| `--intro-test` | Play every animation and exit when done. |
| `--sprite-test` | Show the sprites of the current room (combine with `--room`). |

## Keys

To control your character, use either set of cursor keys. The action keys are
available on both `Z`/`X`/`C` and `A`/`S`/`D`:

- <kbd>Z</kbd>/<kbd>A</kbd>  shoot, keep it down to run
- <kbd>X</kbd>/<kbd>S</kbd>  use your whip
- <kbd>C</kbd>/<kbd>D</kbd>  jump
- <kbd>F5</kbd>              quicksave (one slot)
- <kbd>F7</kbd>              quickload
- <kbd>F2</kbd>              cycle internal upscaling (classic renderer)
- <kbd>F3</kbd>              toggle sharp / smooth rendering
- <kbd>F</kbd>               toggle bilinear filtering, or Omniscale in HD mode
- <kbd>F12</kbd>             save a high-resolution screenshot
- <kbd>ESC</kbd>             leave the game, or skip an animation

### Changing the keys

Every binding above can be changed without rebuilding. On first start the game
writes a documented `keys.txt` next to the executable (or in the current
directory) that looks like this:

```
# walk up
move up = up

# action button A
action a = z, a

screenshot = f12
fullscreen = alt+enter
```

Edit a line and restart. Keys are written by name (`a`, `f12`, `left`,
`space`, `enter`, ...), several keys can be listed separated by commas, and
modifiers are prefixed with `+` (`ctrl+s`, `alt+enter`, `shift+f1`). Leave the
right side empty to unbind an action. Unknown names and actions only print a
warning, so a typo never stops the game from starting. Use `--keys <file>` to
load a different file.

The full list of action names and allowed key names is in the comment header of
the generated `keys.txt`. After editing, run `alien --check-keys` to print
warnings without starting the game.

## Building

CMake is required:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

SDL3 and SDL3_mixer are needed (pkg-config, CMake packages or `CMAKE_PREFIX_PATH`).
OpenGL is optional and only used by the HD renderer; without it the game builds
and falls back to the plain SDL renderer automatically.

The Windows release in this repository is built with MinGW-w64 gcc 16.2,
CMake 4.5 and SDL3 3.4.20 / SDL3_mixer 3.2.4, then packaged with
`play.bat`, `alien.exe`, `SDL3.dll`, `SDL3_mixer.dll`, `README.md`, `COPYING`
and `AUTHORS.md` in a single flat zip.

## Tests

- `tests/run-tests.ps1` — headless Windows suite: 37 checks covering option
  validation, the intro, every room, every upscaler, both renderers, the key
  binding parser and recorded playthroughs. Game data is expected at
  `tests/work/game.iso`.
- `tests/run-asan.sh` — the same idea under AddressSanitizer and UBSan in WSL,
  using the `dummy` SDL video and audio drivers.

## Reporting bugs

Please open an issue at
<https://github.com/jorgesalgueiro/hota-redux/issues>. If you can, attach a
`quicksave` or a `recorded-keys` file — both make a bug much easier to
reproduce.

Some oddities you may find are also present in the original Sega CD game. Only
one has been checked and confirmed faulty on the real hardware as well:

- First screen: run left, quickly run right (away from the beast), then run
  left again — the beast is gone.

## FAQ

- **Q: Why can't I play *Another World* with this?**
  A: This project is only *Heart of the Alien*. *Another World* was attached to
  the Sega CD release as a bonus; play it with a dedicated engine such as
  [Rawgl](https://github.com/cyxx/rawgl) or in DOSBox.

- **Q: Can you send me the game?**
  A: No. You need your own copy of the Sega CD game. This repository contains
  source code and engine assets only, never the game data.

- **Q: The picture is still too soft / too sharp.**
  A: Tune it: `--hd-smooth 15` is subtler, `--hd-smooth 0` or `--sharp` is pure
  nearest-neighbor, `--hd-smooth 100` is full Omniscale. <kbd>F3</kbd> switches
  at runtime.

- **Q: It runs slowly / OpenGL is missing.**
  A: Try `--no-hd`; the SDL renderer needs no OpenGL at all.

## Credits

### The original game

- ***Heart of the Alien: Out of This World Parts I and II*** (Sega CD, 1994),
  developed by **Interplay Productions** and published by **Virgin Interactive
  Entertainment**. A sequel to *Another World*, continuing the story of Buddy.
- ***Another World* / *Out of This World*** (1991), created by **Éric Chahi**,
  developed and published by **Delphine Software International**. Without that
  game there would be no *Heart of the Alien* — and no reason for this port.

### Heart of The Alien Redux (2004-2005)

- **Gil Megidish** — creator of the original *Heart of The Alien Redux*: reverse
  engineering, the virtual machine, game logic, graphics/sound decoding and the
  tools that made a faithful reimplementation possible. Original homepage:
  <https://hota.sourceforge.net>.
- **M-HT** — contributions to the Redux code base.
- Everyone who reported bugs, recorded key sequences and tested builds on the
  SourceForge project over the years.

### The 2.0.0 release and later upstream work

- **carstene1ns** — maintainer of the 2.0.0 release and of the
  <https://github.com/carstene1ns/hota-redux> repository: the SDL3 port, the
  modern CMake build, install/Freedesktop metadata, the intro animation fixes
  and much of the cleanup that this fork builds on. Also credited by upstream
  for later work such as gamepad support and screenshot support.
- The **2.0.0 contributors** and bug reporters whose fixes, reports and testing
  shaped that release; see the upstream repository history and `AUTHORS.md`.
- **Lior Halphon** — author of the **Omniscale** pixel-art shader (MIT), and
  **hunterk** for the RetroArch GLSL port used in `src/omniscale_shader.h`.

### This 2.0.1 fork

- **Jorge Salgueiro** — SDL3/OpenGL HD renderer, adjustable Omniscale
  smoothing, key binding system, renderer/screenshot fixes, the bug fixes
  listed in the changelog, the regression suite, the Windows build and the
  release packaging.
- **opencode** and **DeepSeek** — AI pair-programming used to audit the code,
  implement and tune the renderer and shaders, build the key binding parser and
  test suite, and write the documentation. This fork was made with their help.

### Third-party software

- **SDL3** and **SDL3_mixer** (zlib license) — <https://libsdl.org>.
- **Omniscale** (MIT) — <https://github.com/libretro/glsl-shaders>.
- **CMake** (BSD-3-Clause) and **MinGW-w64** (public domain / permissive) for
  the Windows build.

## License

The engine is free software under the **GNU General Public License v2.0 or
later**; see `COPYING`. The Omniscale shader is MIT licensed, with its license
text kept in `src/omniscale_shader.h`. The game itself, its graphics, music and
story are © their original owners and are **not** covered by this license or
included in this repository.

## Links

- This fork: <https://github.com/jorgesalgueiro/hota-redux>
- Upstream Redux (2.0.0): <https://github.com/carstene1ns/hota-redux>
- Original project: <https://hota.sourceforge.net>
- Original SourceForge project: <https://sourceforge.net/projects/hota/>
- MobyGames entry: <https://www.mobygames.com/game/8140/heart-of-the-alien-out-of-this-world-parts-i-and-ii/>
- *Another World* by Éric Chahi: <https://www.anotherworld.fr/>

## Changelog

### 2.0.1

Picture and controls:

- Added: HD rendering through SDL/OpenGL with the Omniscale pixel-art shader
  (MIT, by Lior Halphon; see `src/omniscale_shader.h`), on by default
- Added: `--hd-smooth 0-100` dials the Omniscale strength, so diagonal edges
  lose their stair-step wobble with only a light touch of smoothing (default 30)
- Added: high-resolution internal upscaling with a sharp nearest-neighbor default
- Added: optional Scale2x/3x smoothing (`--smooth` or F3)
- Added: automatic window size that fits the desktop
- Added: F2 cycles the upscaler, F toggles filtering, F12 saves high-res screenshots
- Added: HD screenshots are rendered offscreen, so they work with an occluded window
- Added: user configurable key bindings in a documented `keys.txt`
  (`--keys`, `--check-keys`)
- Added: `play.bat` launcher for the Windows release

Fixes:

- Fixed: the HD quad left a large black triangle unpainted (wrong triangle strip order)
- Fixed: the GL renderer no longer blocks the event loop while the window is hidden
- Fixed: uninitialized screen offset during cutscenes could crash on optimized builds
- Fixed: out-of-bounds shift in the LZSS backdrop decoder
- Fixed: task ranges starting above task 0 were enabled/disabled in the wrong slot
- Fixed: replaying without a recorded-keys file, and closing the window while not recording
- Fixed: one-shot music tracks looping forever
- Fixed: out-of-bounds reads when a corrupt/invalid sprite is drawn
- Fixed: unchecked buffer sizes for ISO/music paths and out-of-range quicksaves
- Fixed: keyboard auto-repeat triggering save/load/screenshot repeatedly
- Added: automatic ISO detection when loose game files are not present

Build and testing:

- Added: Windows x64 release zip (`alien.exe`, SDL3 DLLs, docs, `play.bat`)
- Added: `tests/run-tests.ps1` smoke/regression suite (37 checks)
- Added: `tests/run-asan.sh` sanitizer suite for Linux/WSL

### 2.0.0

- Ported to SDL3
- Fixed: no more intro animation bugs

### 1.2.4

- Added: joystick support
- Fixed: was polling one event at a time
- Fixed: action keys break animation (instead of esc)
- Fixed: better timing
- Fixed: crash at getopt when parsing invalid command line (getopt)
- Added: support for A,S,D keys (German keyboards :)
- Lots of code cleanups

### 1.2.2

- Added: scaler2x effect
- Added: scaler3x effect
- Added: simple x3 scaler
- Fixed: sound effects
- Major code cleanups

### 1.2.0

- First public release!
