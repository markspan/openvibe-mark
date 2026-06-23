# OpenViBE — UG-Lab Build Notes

## What was changed and why

Two binaries in this build differ from upstream OpenViBE:

### `openvibe-acquisition-server.exe`

**Problem:** The TMSi/Porti driver processed all `CHANNELTYPE_DIG` channels for trigger detection, including the SAW (ramp) channel which caused spurious stimulation events on every sample. In addition, the acquisition server forwarded all internal stimulation events (drift correction, artefact markers, etc.) to connected clients.

**Fix (in `meta/extras/applications/platform/acquisition-server-legacy-2023/src/`):**

- `drivers/tmsi/ovasCTMSiAccess.cpp` — only process DIG channels that are activated in the acquisition server UI; send trigger values as `OVTK_StimulationId_Label(value)` (range `0x00008101–0x000081FF`).
- `ovasCAcquisitionServer.cpp` — filter `setStimulationSet()` to only forward stimulations in the label range, dropping all internal server events.

### `openvibe-plugins-contrib-file-io.dll`

**Problem:** The BrainAmp file writer (`BrainampFileWriterUG` box) wrote every stimulation it received to the `.vmrk` marker file, including internal OpenViBE events.

**Fix (in `meta/extras/contrib/plugins/processing/file-io/src/box-algorithms/`):**

- `ovpCBoxAlgorithmBrainampFileWriterUG.cpp` — only write stimulations in the `OVTK_StimulationId_Label` range (`0x00008101–0x000081FF`) and write the raw trigger value (`id - 0x00008100`, i.e. 1–255) to the `.vmrk` file.

### `openvibe-plugins-simple-visualization.dll`

**Problem:** The signal display box showed all stimulations it received, including internal OpenViBE events that the acquisition server filter blocks at source. If a scenario bypasses the server filter (e.g. a test pipeline), spurious events would appear.

**Fix (in `meta/extras/plugins/processing/simple-visualization/src/box-algorithms/`):**

- `CSignalDisplay.cpp` — filter the stimulation loop to only pass stimulations in the `OVTK_StimulationId_Label` range (`0x00008101–0x000081FF`) to the display database.

---

## How to rebuild

All sources live under `meta/`. The build requires a conda environment and a 64-bit Visual Studio developer shell.

### One-time setup: create the conda environment

Open an **Anaconda Prompt** and run:

```cmd
cd meta
conda env create --file conda\env_windows.yaml
```

### Rebuilding

Every time you want to compile, open an **Anaconda Prompt** and run the following sequence:

```cmd
:: 1. Activate the conda environment
conda activate openvibe-370

:: 2. Enter the 64-bit Visual Studio developer shell
for /f "usebackq tokens=*" %i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%i\VC\Auxiliary\Build\vcvarsall.bat" x64

:: 3. Re-prepend conda to PATH so its cmake/ninja take priority over VS tools
set PATH=%CONDA_PREFIX%\Library\bin;%CONDA_PREFIX%\Scripts;%CONDA_PREFIX%;%PATH%

:: 4. Configure (only needed once, or after CMakeLists changes)
cd meta
rmdir /s /q build
mkdir build
cd build
cmake .. -G Ninja -DCMAKE_PREFIX_PATH=%CONDA_PREFIX%\Library

:: 5. Build
ninja
```

For incremental rebuilds (source changes only, no CMakeLists changes), skip step 4 and just run `ninja` from the `meta\build\` directory.

### Outputs

After a successful build the relevant files are in `meta\build\bin\`:

| File | Deploy to lab as |
|---|---|
| `openvibe-acquisition-server.exe` | Replace acquisition server executable |
| `openvibe-plugins-contrib-file-io.dll` | Replace contrib file-io plugin DLL |
| `openvibe-plugins-simple-visualization.dll` | Replace simple-visualization plugin DLL |
