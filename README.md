# Asura's Wrath Recompiled

PC port of Asura's Wrath (`default.xex`, Title ID `43430817`) via Xbox 360 static PowerPC recompilation to C++23.

> Note: Android port is experimental, use on your own risk.

<a href="https://www.youtube.com/playlist?list=PLVyuezqTsTB0">
        <img alt="cover" src="https://github.com/user-attachments/assets/339f7f1b-85d4-48ac-9b5f-ac4fad276dad" />
</a>

https://www.youtube.com/playlist?list=PLVyuezqTsTB0

> Original illustration by Satoshi Sakai

---

## 1. Requirements

- A copy of the Xbox 360 game ISO (`Asura's Wrath.iso`).
- [extract-xiso](https://github.com/XboxDev/extract-xiso) tool to extract `.iso` contents.
- CMake 3.25+ and Clang / Ninja compiler toolchain.

---

## 2. Setup

### Clone Repository
```bash
git clone --recursive https://github.com/PauloUbirajara/asuras-wrath-recomp.git
cd asuras-wrath-recomp
```

---

## 3. Build

> Requires the .iso contents in extracted/ to build.

### Option A: Using CMake Presets (Linux / Windows / Android)

#### Linux
```bash
cmake --preset linux-amd64-release
cmake --build out/build/linux-amd64-release --parallel
```

#### Windows
```cmd
cmake --preset win-amd64-release
cmake --build out/build/win-amd64-release --parallel
```

#### Android
```bash
cmake --preset android-aarch64-release
cmake --build out/build/android-aarch64-release --parallel
```

---

### Option B: Using Build Scripts

#### Linux
```bash
# Build binary
./build.sh

# Build binary and create release zip archive in out/ directory
./build.sh --package

# out/
# ├── build
# │   └── linux-amd64-release
# └── dist
#     ├── asura_wrath_recomp_linux_x86_64
#     └── asura_wrath_recomp_linux_x86_64.zip
```

#### Windows
```ps1
# Build binary
build.bat

# Build binary and create release zip archive in out/ directory
build.bat --package

# out/
# ├── build
# │   └── win-amd64-release
# └── dist
#     ├── asura_wrath_recomp_win_amd64.exe
#     └── asura_wrath_recomp_win_amd64.zip
```

#### Android
```bash
# Build release APK
./build_android.sh

# Build debug APK
./build_android.sh --debug

# out/dist/asura_wrath_recomp_android.apk
```

---

## 4. Game Setup

In the same directory as the executable, run the following:

```bash
# Extract Game ISO
mkdir -p extracted/
extract-xiso -x asura_wrath.iso -d extracted/

# (Optional) Install DLC Content (Not supported/tested yet)
```

If extracted game is somewhere else, it can be specified via the `--game_data_root` argument:

```bash
asura_wrath_recomp --game_data_root=/path/to/extracted/game
```

Available ReXGlue Path Configuration Flags:
| Flag | Description | Default |
| - | - | - |
| `--cache_root` | Directory for compiled Vulkan pipeline and shader storage. | `cache` |
| `--game_data_root` | Root directory containing extracted game files. | `extracted` (if exists) or current directory |
| `--metadata_root` | Directory for game metadata. | `(empty)` |
| `--update_data_root` | Directory containing game update / DLC patches. | `(empty)` |
| `--user_data_root` | Directory for user save games and profile data. | `%USERPROFILE%\Documents\asura_wrath_recomp` (Windows) |
| `--user_data_root` | Directory for user save games and profile data. | `~/.local/share/asura_wrath_recomp` (Linux) |

---

## 5. Run

### Expected Folder Structure Before Running

#### Linux
```
├── asura_wrath_recomp  # Executable
├── extracted/          # .iso contents 
├── librexgpu-xenos.so
├── librexruntime.so
└── libTracyClient.so
```

#### Windows
```
├── asura_wrath_recomp.exe # Executable
├── extracted/             # .iso contents 
├── rexgpu-xenos.dll
├── rexruntime.dll
└── TracyClient.dll
```

### Save Locations

| OS | Location |
| :--- | :--- |
| Linux | `~/.local/share/asura_wrath_recomp` |
| Windows | `%USERPROFILE%\Documents\asura_wrath_recomp\` |
| Android | `<user picks the save location>` |

### Linux
```bash
asura_wrath_recomp

# Linux (What currently works best for me)
asura_wrath_recomp \
    --async_shader_compilation=true \
    --render_target_path_vulkan=fsi \
    --vulkan_async_skip_incomplete_frames=true \
    --vulkan_device=1 \
    --vulkan_pipeline_creation_threads=6 \
    --window_height=1080 \
    --window_width=1920
```

### Windows
```cmd
asura_wrath_recomp.exe
```

### Android
```bash
# Install the game on your phone/emulator
adb install -r out/dist/asura_wrath_recomp_android.apk

# Run the game
adb shell am start -n asura_wrath.recomp/.MainActivity
```

## 6. Custom Run Flags

#### Custom Backend
```cmd
asura_wrath_recomp.exe --gpu_backend=vulkan
asura_wrath_recomp.exe --gpu_backend=d3d12
```

#### Custom Resolution
```bash
asura_wrath_recomp --window_width=1920 --window_height=1080
```

#### Selecting GPU Device

List available GPUs and their index numbers on your system:
```bash
vulkaninfo --summary | grep -E 'GPU[0-9]+:|deviceName'
```

Output:
```text
GPU0:
        deviceName         = Intel(R) UHD Graphics (CML GT2)
GPU1:
        deviceName         = NVIDIA GeForce GTX 1650
GPU2:
        deviceName         = llvmpipe (LLVM 20.1.8, 256 bits)
```

Pass `--vulkan_device=<index>` to target a specific GPU (e.g., index `1` for discrete NVIDIA GPU):
```bash
asura_wrath_recomp --vulkan_device=1
```

#### Render Target Path (`fsi` vs `fbo`)

- `fbo`: Host framebuffers (default, higher performance, can produce color artifacts on some GPUs).
- `fsi`: Fragment Shader Interlock (software EDRAM pixel packing, fixes red overlay artifacts on NVIDIA GPUs).

Fix red overlay artifact on NVIDIA GPUs:
```bash
asura_wrath_recomp --vulkan_device=1 --render_target_path_vulkan=fsi
```

Switch back to host framebuffers:
```bash
asura_wrath_recomp --render_target_path_vulkan=fbo
```

---

## 7. Credits

- Powered by **ReXGlue** (https://github.com/rexglue/rexglue-sdk), an open-source static recompilation framework for Xbox 360 software.
- UI touch controls made possible by using **kenney.nl** Xbox gamepad assets (https://kenney.nl/assets/input-prompts)

---

## 8. Issues

- Had to use FSI over FBO to avoid red artifacts (performance loss / stutters due to on-demand texture / shader compilation - also seen in https://github.com/rexglue/rexglue-sdk/blob/71782a3bc15cd1994381757fae7d616242f22e6a/src/graphics/vulkan/render_target_cache.cpp#L44-L63)

---

## 9. TODO

- [ ] Add touch controls for Android (?)
- [ ] Improve performance
- [ ] Make DLCs work
- [ ] Test for Android (?)
- [ ] Test for Windows (?)
- [x] Make consistent builds
- [x] Make it easier to build & run

## Enhancements

Game-specific options live in `src/asura_patches.cpp` as hooks on recompiled
functions (nothing in `generated/` is edited). All of them are cvars: set them
in `asura_wrath_recomp.toml` next to the executable (see
`asura_wrath_recomp.example.toml`), pass them as `--name=value`, or change them
live in the settings overlay (**F4**).

| Option | Default | What it does |
| --- | --- | --- |
| `asura_60fps` | `false` | 60 FPS with correct game speed (present interval 2 → 1 and constant game-speed scale). **F6** toggles 30/60 live, e.g. for a mash QTE. |
| `asura_disable_postprocess` | `false` | Skips the post-process passes that over-correct edges and smear colors at `resolution_scale` > 1. Changes the look. |
| `input_press_latch` | `true` | A button press that starts and ends between two reads of the pad (stutter, frame drop, fast mashing) is replayed as its own press instead of being lost. |
| `input_press_latch_max` | `6` | Presses buffered per button between two reads. |
| `input_press_latch_hold_ms` | `12` | Minimum time a replayed edge stays visible, for code that polls several times per frame. |

Useful engine options: `resolution_scale` (2 = 1440p, 3 = 2160p internal),
`anisotropic_override=5` (16x), `user_language=4` (French), `vsync=true`
(guest pacing follows the emulated 60 Hz vblank, independent of the monitor).

Patch addresses come from the Xenia Canary game patch for this title (author:
boma) and target the base XEX (`904B784109438B34`). `tools/re/` has small
helpers to inspect the generated code: `fn.py sub_XXXXXXXX [-a]` prints a
function (`-a`: PPC listing only), `addr.py 8XXXXXXX` finds the function that
contains an address, `insn.py 8XXXXXXX` shows the C emitted for one
instruction.

### Windows startup crash (0xC0000409)

`rex::filesystem::GetExecutablePath` used `_get_wpgmptr`, which is only set
when the CRT entry point is `wmain`; otherwise the CRT invalid-parameter
handler kills the process before anything is logged. The SDK fork now uses
`GetModuleFileNameW`.
