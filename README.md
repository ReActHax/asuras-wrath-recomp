# Asura's Wrath Recompiled — enhanced fork

[Français](README.fr.md)

<img alt="Asura's Wrath" src="https://github.com/user-attachments/assets/339f7f1b-85d4-48ac-9b5f-ac4fad276dad" />

*Original illustration by Satoshi Sakai.*

A native Windows PC version of **Asura's Wrath** (Xbox 360, title ID `43430817`),
made by statically recompiling the original PowerPC executable to C++ with the
[ReXGlue](https://github.com/rexglue/rexglue-sdk) SDK.

This is a fork of [PauloUbirajara/asuras-wrath-recomp](https://github.com/PauloUbirajara/asuras-wrath-recomp)
that adds stability fixes, title update 1 and DLC support, 60 FPS, quality of
life options, a launcher/installer and texture replacement.

> **No game files are included.** You need your own copy of the game (Xbox 360
> disc image) and its title update 1.

> **This project is not maintained.** It is published as is, as a finished
> experiment. Issues and pull requests will probably not be answered — feel free
> to fork it.

---

## Contents

1. [Features](#features)
2. [Playing (download and install)](#playing-download-and-install)
3. [Launcher and settings](#launcher-and-settings)
4. [In-game keys](#in-game-keys)
5. [Texture replacement (HD textures)](#texture-replacement-hd-textures)
6. [Known issues](#known-issues)
7. [Building from source](#building-from-source)
8. [Project layout](#project-layout)
9. [Credits](#credits)
10. [Legal](#legal)

---

## Features

Compared to the original recompilation:

**Fixes**
- Startup crash on Windows (`0xC0000409`, executable path).
- Random crashes (heap corruption): the controller list was rebuilt from
  several threads without a lock.

**Game content**
- **Title update 1** built in (fixes soft locks, e.g. in episode 7).
- **DLC**: packages are installed automatically at launch.

**Enhancements**
- **60 FPS** with the correct game speed (toggle 30/60 live with F6).
- **QTE-friendly input**: no press is lost during a slowdown, and an optional
  **turbo** (hold a button instead of mashing it) for mashing QTEs, which are
  harder at 60 FPS.
- **Skip the startup logos.**
- **Effect toggles**: motion blur, depth of field, bloom, ambient occlusion,
  light shafts, lens flares, distortion, radial blur, plus experimental sharper
  textures and shadows.
- **AMD FidelityFX** upscaling (CAS, FSR) and higher internal resolutions.
- **Cheat menu (F9)**: memory search (Cheat Engine style), editable/frozen
  values, god mode and instant burst.
- **Texture replacement**: swap any texture with your own DDS file, at any
  resolution — the base for HD texture packs (none is included).
- **Launcher / installer**: installs the game files from your disc image and
  edits the settings, in English or French.

---

## Playing (download and install)

Requirements: Windows 10/11 64-bit, a DirectX 12 graphics card, about 7 GB of
free space, and:

- your **Asura's Wrath disc image** (`.iso`, Xbox 360);
- **title update 1**, file `TU_11K620N_000000K000000.0000000000085`
  (required: the recompiled code targets this version);
- optionally your **DLC** packages (the files from `Content\0000000000000000\43430817\00000002`
  on an Xbox 360 or from your backups).

Steps:

1. Download the latest `AsurasWrathRecompiled_windows_x64.zip` from the
   [Releases page](https://github.com/ReActHax/asuras-wrath-recomp/releases).
2. Extract it to a folder of your choice (not inside `Program Files`).
3. Run **`AsuraLauncher.exe`**. On first launch it asks for the disc image,
   the title update file and the DLC folder, then extracts and checks
   everything (a few minutes).
4. Adjust the settings if you like, then click **Play**.

Saves, logs, DLC and textures are stored in `Documents\asura_wrath_recomp`.

To update, extract a new release over the old folder: the game files
(`extracted\`) and your settings (`asura_wrath_recomp.toml`) are kept.

---

## Launcher and settings

`AsuraLauncher.exe` edits `asura_wrath_recomp.toml` next to the game (comments
and unknown options are kept). Every option is explained in the launcher and in
[`asura_wrath_recomp.example.toml`](asura_wrath_recomp.example.toml).

| Tab | Options |
| --- | --- |
| Display | fullscreen, internal resolution (1×–3×), upscaling (bilinear, CAS, FSR), anisotropic filtering, vsync, renderer (D3D12/Vulkan), game language |
| Game | 60 FPS, skip logos, disable post-processing |
| Effects | motion blur, depth of field, bloom, ambient occlusion, light shafts, lens flares, distortion, radial blur, texture detail, high resolution shadows |
| Controls | remember quick presses (QTE), turbo, turbo buttons and rate |
| Textures | texture replacement, texture dump, open the textures folder |
| About | in-game keys, data folder, install / repair the game files |

Recommended: internal resolution 1× with **FSR 1** upscaling. Higher internal
resolutions look sharper but cost a lot of performance, because the Xbox 360
video memory (EDRAM) has to be emulated at that resolution.

Every option can also be passed on the command line, for example
`asura_wrath_recomp.exe --resolution_scale=2 --gpu_backend=d3d12`.

---

## In-game keys

| Key | Action |
| --- | --- |
| F4 | Settings overlay |
| F6 | Toggle 30 / 60 FPS |
| F8 | Toggle turbo |
| F9 | Cheat menu |
| Alt+Enter | Fullscreen |

---

## Texture replacement (HD textures)

Any 2D texture of the game can be replaced by a DDS file of any size: dump the
textures, edit or upscale them, and drop them in `textures\replace`. No texture
pack is included. See [docs/TEXTURE_MODDING.md](docs/TEXTURE_MODDING.md).

---

## Known issues

- **Vulkan** shows many graphical glitches in this game (lighting, effects):
  use Direct3D 12.
- Internal resolutions above 1× are very demanding.
- `fsr2` / `fsr3` are not true temporal upscalers here (no motion vectors).
- `asura_fx_texture_detail` and `asura_fx_high_shadows` are experimental and
  use more memory.
- Ultrawide is not supported (fixed 720p game, HUD and cutscenes).

---

## Building from source

For developers. Players should use the [releases](https://github.com/ReActHax/asuras-wrath-recomp/releases).

Requirements:
- Visual Studio 2022 (or Build Tools) with *Desktop development with C++*,
  *C++ Clang tools for Windows* and *C++ CMake tools*;
- Git;
- the game files: your extracted disc image plus `default.xexp` from title
  update 1, in `extracted\` at the repository root (the code generator reads
  `extracted\default.xex` and applies `default.xexp`). The easiest way is to
  let `AsuraLauncher.exe` from a release install them, then copy or link its
  `extracted` folder.

```bat
git clone --recursive https://github.com/ReActHax/asuras-wrath-recomp.git
cd asuras-wrath-recomp
rem extracted\ must contain default.xex, default.xexp and BCGame\
package_release.bat
```

Run `package_release.bat` from the *Developer Command Prompt for VS 2022*. It
configures and builds with CMake (preset `win-amd64-release`, D3D12 + Vulkan +
FidelityFX), copies the game to `out\dist\asura_wrath_recomp_windows_x86_64`,
builds the launcher with the C# compiler included in Windows and creates the
release archive in `out\release`. The archive never contains game files.

Manual build:

```bat
cmake --preset win-amd64-release -DREXGLUE_USE_VULKAN=ON -DREXGLUE_USE_D3D12=ON -DREXGLUE_ENABLE_FIDELITYFX=ON
cmake --build out/build/win-amd64-release --parallel
```

Technical notes (how the patches work, addresses, pitfalls): [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md).

Linux and Android builds come from the original project and were not tested
with this fork's changes.

---

## Project layout

| Path | Contents |
| --- | --- |
| `src/asura_patches.cpp` | 60 FPS, post-process toggle, logo skip, hotkeys |
| `src/asura_config_patch.cpp` | effect toggles (patched copies of the game's `Coalesced_*.bin` config) |
| `src/asura_cheats.cpp` | F9 cheat menu and memory search |
| `src/asura_trainer.cpp` | god mode and instant burst |
| `src/asurawrath_app.h` | application entry, paths, game data preparation |
| `includes/funcs.toml` | function addresses for title update 1 |
| `generated/` | recompiled game code (from the code generator) |
| `tools/rexglue` | ReXGlue SDK fork ([ReActHax/asura-wrath-rexglue-sdk](https://github.com/ReActHax/asura-wrath-rexglue-sdk)): texture replacement, turbo, DLC install, crash fixes… |
| `tools/launcher` | `AsuraLauncher.cs` (launcher / installer) |
| `tools/re` | small helpers to read the generated code |
| `docs/` | texture modding guide, developer notes |

---

## Credits

- **PauloUbirajara** — original Asura's Wrath recompilation and SDK fork.
- **Tom Clay and contributors** — [ReXGlue](https://github.com/rexglue/rexglue-sdk) SDK.
- **Ben Vanik and the Xenia contributors** — [Xenia](https://github.com/xenia-project/xenia), on which the ReXGlue runtime is based.
- **boma** — Xenia Canary game patches for Asura's Wrath (60 FPS, post-process),
  used to find the patched functions ([xenia-canary/game-patches](https://github.com/xenia-canary/game-patches)).
- **AxR / RETROBYTE / XYZEngine** — Aurora trainer for Asura's Wrath, from which
  the god mode and instant burst cheats were reproduced.
- **AMD** — FidelityFX (CAS, FSR).
- **kenney.nl** — gamepad assets used by the original Android port.
- **ReActHax** — this fork: testing, direction, startup crash fix.
- Most of this fork's code and documentation was written with the help of
  **Claude** (Anthropic), an AI assistant, under ReActHax's direction and testing.

---

## Legal

Asura's Wrath is © CAPCOM CO., LTD. This project is not affiliated with or
endorsed by Capcom, CyberConnect2 or Microsoft. No game assets are distributed:
you must own the game and provide your own disc image and title update. Do not
use this project for piracy.

The original project has no license file; the ReXGlue SDK and Xenia are under
the BSD 3-Clause license (see `tools/rexglue/LICENSE`). This fork's own changes
are provided as is, without warranty.
