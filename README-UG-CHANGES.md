# OpenViBE UG Modifications

Changes made to the OpenViBE 3.7.0 source for the UG acquisition setup.

---

## New box: Signal Display (UG)

**DLL:** `openvibe-plugins-simple-visualization.dll`  
**Class:** `CSignalDisplayUG` (in `CSignalDisplayUGDesc`)  
**Category:** `Visualization/Basic`  
**Source files:**
- `meta/extras/plugins/processing/simple-visualization/src/box-algorithms/CSignalDisplayUG.hpp`
- `meta/extras/plugins/processing/simple-visualization/src/box-algorithms/CSignalDisplayUG.cpp`
- Registered in `src/main.cpp` via `OVP_Declare_New(CSignalDisplayUGDesc)`
- Identifiers in `src/defines.hpp`: `Box_SignalDisplayUG` / `Box_SignalDisplayUGDesc`

### Behaviour differences from the standard Signal Display

| Feature | Standard | UG |
|---|---|---|
| Stimulation filter | all codes | only `OVTK_StimulationId_Label` values 1–255 (i.e. `0x00008101`–`0x000081ff`) |
| Stimulation lines | per-code colour | uniform orange, dotted |
| Stimulation label | colour legend | numeric value in a box at foot of line |
| Stimulation Colors toolbar button | shown | hidden |

### Stimulation label box

- Fixed width (36 px) regardless of widget or font size.
- Light-blue fill at **25% opacity** (semi-transparent, overlapping boxes show through).
- Dark-blue solid 1 px outline.
- Black text, 11 pt Sans, centred in box.
- Box is **left-anchored**: right edge sits on the trigger line, box extends leftward.  
  This keeps the box inside the already-drawn scan strip so new data never overwrites it.
- Drawing is deferred via `g_idle_add` (`DrawUGLabelsIdle`) so it fires *after*
  GTK composites the double-buffer back-buffer to screen — bypassing the narrow
  back-buffer clip that would otherwise truncate the box in scan mode.

### Key implementation notes

- `CSignalDisplayView::SetUGMode(bool)` propagates the flag to all
  `CSignalChannelDisplay` instances (including those created later in `Init()`).
- Label drawing uses **Cairo** (`gdk_cairo_create` + `pango_cairo_show_layout`)
  for alpha compositing; the trigger line itself still uses GDK.
- The `pangocairo.h` include was added to `CSignalChannelDisplay.cpp`.

---

## Test Lua stimulator

**File:** `meta/extras/plugins/processing/stimulation/share/ug-trigger-stimulator.lua`  
(also copied to `meta/build/share/openvibe/plugins/stimulation/`)

Emits codes 1–255 encoded as `OVTK_StimulationId_LabelStart + code` at a fixed
interval. Edit `TRIGGER_CODES` and `TRIGGER_INTERVAL` at the top of the file.

---

## VS Code setup

Config lives in `.vscode/` at the repo root:

| File | Purpose |
|---|---|
| `settings.json` | Points CMake Tools and IntelliSense at `meta/build`; sets default terminal to conda env `openvibe-370` |
| `c_cpp_properties.json` | Points IntelliSense at `meta/build/compile_commands.json` |
| `tasks.json` | `Ctrl+Shift+B` builds `simple-visualization` and refreshes `compile_commands.json` |
| `extensions.json` | Recommends C/C++, CMake Tools, CMake, Error Lens, GitLens |

`compile_commands.json` is generated via `ninja -t compdb` (not cmake, to avoid
triggering the pre-existing pthreads link-check failure in the cmake re-configure step).
The build task does this automatically after each build.

### Building from the command line

```cmd
:: From an Anaconda Prompt or any prompt with ninja on PATH:
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd meta\build
ninja openvibe-plugins-simple-visualization
```

`ninja` is at:
`C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe`
(added to user PATH).

### Conda environment

The build links against headers and libraries from the `openvibe-370` conda env
(`C:\Users\P154492\AppData\Local\anaconda3\envs\openvibe-370`).  
All paths are baked into `build.ninja` and `compile_commands.json`.

---

## Gotchas

- **Do not edit `CMakeCache.txt` directly.** Doing so causes ninja to trigger a
  cmake re-configure, which fails on a pthreads link check. If this happens,
  touch `meta/build/build.ninja` to reset the timestamp and prevent regeneration.
- **New `.cpp` files need `cmake ..` to be picked up** — CMake uses `GLOB_RECURSE`
  for source files, so the glob cache must be refreshed after adding files.
- The `CSignalDisplayUG` stimulation filter (`id < LabelStart+1 || id > LabelEnd`)
  is applied in the `process()` loop before codes reach `CBufferDatabase`, so
  non-hardware-trigger stimulations never appear in the UG display at all.
