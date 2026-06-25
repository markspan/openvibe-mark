# EEGO-UG Acquisition Server Driver

Orientation for resuming work on the EEGO-UG driver in this repo.

## What was done

The original `eemagine-eego` contrib driver was redesigned and renamed to `EEGO-UG`. The core change: hex bitmask channel selection was replaced by a channel-names dialog, making it work like the TMSi driver. Everything is compiled into `openvibe-acquisition-server.exe`.

## File locations

| What | Where |
|---|---|
| Driver source | `meta/extras/contrib/plugins/server-drivers/eemagine-eego-ug/src/` |
| UI file (source) | `meta/extras/contrib/plugins/server-drivers/eemagine-eego-ug/share/interface-EEGO-UG.ui` |
| UI file (runtime) | `meta/build/share/openvibe/applications/acquisition-server/interface-EEGO-UG.ui` |
| Driver registration | `meta/extras/contrib/common/contribAcquisitionServer.inl` |
| CMake include path | `meta/extras/contrib/common/contribAcquisitionServer.cmake` |
| SDK find module | `meta/CMake/drivers/FindThirdPartyEemagineEEGO.cmake` (unchanged, SDK-level) |

## Class names

| C++ identifier | Role |
|---|---|
| `CDriverEEGO_UG` | Main driver, registered in `.inl`, appears as "EEGO-UG" in the AS dropdown |
| `CConfigurationEEGO_UG` | Configuration dialog handler |
| `CHeaderEEGO_UG` | Settings/header storage (EEG range, BIP range, masks) |

## How channel selection works

1. User opens configuration, clicks **Change channel names**.
2. The spinbutton upper bound is set from the live amplifier's `getChannelList()` count (falls back to 64 EEG + 24 BIP if not connected).
3. User types names into slots they want to record. Slots left blank are skipped.
4. **Slot mapping**: slots 0..nEEG-1 = EEG (referential) channels; slots nEEG..nEEG+nBIP-1 = BIP channels.
5. On Apply, `CConfigurationEEGO_UG::postConfigure()` iterates all named slots, builds `eegMask` and `bipMask` uint64 bitmasks, compacts the channel name list, and stores the masks in `CHeaderEEGO_UG`.
6. The driver passes those masks to `amplifier::OpenEegStream(freq, eegRange, bipRange, eegMask, bipMask)`.

## Live SDK queries at configure time

In `CDriverEEGO_UG::configure()`, before showing the dialog, the driver tries to:
- `factory().getAmplifier()` — get a temporary amplifier handle
- `amp->getChannelList()` — count `reference` and `bipolar` channels
- `amp->getSamplingRatesAvailable()` — populate the sampling rate combobox
- `amp->getReferenceRangesAvailable()` — populate the EEG range combobox (volts x1000 = mV)
- `amp->getBipolarRangesAvailable()` — populate the BIP range combobox

If the amplifier is not connected a warning dialog appears ("Cannot find an EEGO amplifier. Is it connected and switched on?") and the dialog opens with hardcoded fallback values.

## Things NOT yet tested with hardware

- Live `getAmplifier()` call in configure path
- Sampling rate / range combobox population from SDK
- Channel name -> mask -> stream round trip with real data
- Impedance check mode

## Build

From an Anaconda Prompt with the conda env active and VS environment initialised:

```cmd
set VCToolsVersion=
set VSCMD_VER=
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
set PATH=%CONDA_PREFIX%\Library\bin;%CONDA_PREFIX%\Scripts;%CONDA_PREFIX%;%PATH%
cd meta\build
ninja openvibe-acquisition-server
```

If the build directory is stale (e.g. after a VS update), wipe `meta/build`, re-run `cmake .. -G Ninja -DCMAKE_PREFIX_PATH=%CONDA_PREFIX%\Library`, then `ninja`.

The `VCToolsVersion=` clear is needed because the conda activation script pins the old toolset version.

## SDK notes

The eemagine SDK is at `%CONDA_PREFIX%\Library\include\eemagine\include\eemagine\sdk\`. Key headers:

- `amplifier.h` — `getChannelList()`, `getSamplingRatesAvailable()`, `getReferenceRangesAvailable()`, `getBipolarRangesAvailable()`, `OpenEegStream()`, `OpenImpedanceStream()`
- `channel.h` — `channel::channel_type` enum: `reference`, `bipolar`, `trigger`, `sample_counter`, ...
- `factory.h` — entry point: `factory(dllPath).getAmplifier()`

Ranges are in **volts** in the SDK; the UI shows **mV** (multiply by 1000).

## Known issues / future work

- Sampling rate and range dropdowns in the UI still contain hardcoded fallback values (the UI GtkListStores). These are cleared and repopulated at runtime when the amplifier is connected, so the hardcoded values only matter if the device is absent at configure time.
- The `check_configuration()` method still exists and may show a confusing dialog if the mask count mismatches the header channel count. This path should not normally be hit with the new name-based flow.
- No hardware test yet (as of 2026-06-25).
