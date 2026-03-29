# NINJAM Next Plugin

A modern JUCE-based VST3/AU plugin for [NINJAM](https://www.cockos.com/ninjam/) — collaborative real-time music jamming over the internet, right inside your DAW.

## Features

- VST3 (Windows + macOS) and AU (macOS) plugin formats
- Host transport sync with ring-buffer phase alignment
- Classic NINJAM mode with auto-subscribe
- Built-in metronome aligned to DAW beats
- Mixer-style channel strips with per-user mute/solo/level
- Chat/log panel with scrollable history
- Interval position readout as bars:beats:subbeats (`1:1:1`) using host time signature
- MIDI input/output passthrough (for DAW routing workflows)

## How To Use

1. Insert the plugin on a track or bus and open the editor.
2. Enter `Host`, `User`, and `Password`, then click **Connect**.
3. Use the chat box at the bottom to send messages:
   - Regular text sends chat messages.
   - `/...` sends admin commands (example: `/bpm 120`).
4. Adjust monitoring/mix from the top strip (`Me`) and remote user strips.
5. Watch `Interval: bar:beat:subbeat (num/den)` for position within the current NINJAM interval.

`subbeat` follows a 1/16-note grid relative to the host time signature:
- `4/4` -> `bar:1-4:1-4`
- `3/4` -> `bar:1-3:1-4`
- `6/8` -> `bar:1-6:1-2`

When a server presents a license agreement, it is shown in the chat/log panel and connection is paused until you approve.
Click `License OK` in the plugin UI to approve that host's license and reconnect.

If an open server rejects a normal username (empty password), the plugin retries once as
`anonymous:<your-name>` automatically.

## Monitoring Buttons (`A` / `L`)

- `A` = **Add local**: hear your local input mixed with incoming NINJAM audio.
- `L` = **Listen local**: hear only your local input (incoming NINJAM audio muted).
- Both off = **Incoming only**: hear remote NINJAM audio only.

`A` and `L` are mutually exclusive monitor modes.

## Example: Plugin On Main Bus As Mixer

Use this when you want the plugin to behave like a NINJAM return + local confidence monitor:

1. Put **NinjamNext** on your DAW master/main bus.
2. Route your instrument/vocal tracks to the master as normal.
3. Connect to the NINJAM server.
4. Turn on `A` in the `Me` strip.
5. Keep remote user strips up and mix each user with mute/solo/level.

Result: you hear the full main mix plus your local input confidence monitor, while still controlling remote users from one place.

## Build

### Prerequisites

- CMake 3.22+
- **Windows**: Visual Studio 2022+ with C++ workload
- **macOS**: Xcode command line tools
- **macOS**: Homebrew + CMake if `cmake` is not already installed

### Clone & Build

```bash
git clone --recursive https://github.com/nykwil/ninjam-next-plugin.git
cd ninjam-next-plugin
```

#### Windows

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target NinjamNext_VST3
```

On Windows, run those commands from a **Visual Studio Developer Command Prompt** (or use `build_win.bat` below).  
If you see `fatal error C1083: Cannot open include file: 'algorithm'`, the MSVC environment was not initialized.

On Windows you can also use the one-click script:

```powershell
build_win.bat
```

#### macOS

If needed, install the required tools first:

```bash
xcode-select --install
brew install cmake
```

Then build:

```bash
./build_mac.sh
```

This configures CMake and builds both the VST3 and AU targets.
The CMake 4.x compatibility workaround is built into the project now, and macOS defaults to a universal binary (`arm64;x86_64`).

If you want the raw CMake commands instead of the helper script:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target NinjamNext_VST3 NinjamNext_AU
```

### Output

- **VST3**: `build/NinjamNext_artefacts/<config>/VST3/NinjamNext.vst3/`
- **AU** (macOS): `build/NinjamNext_artefacts/<config>/AU/`

On macOS, the simplest install path is:

```bash
./install_mac.sh
```

That copies the built bundles into your user plugin directories, clears extended attributes, and applies ad-hoc signing when available.

Default user install locations:

- **VST3**: `~/Library/Audio/Plug-Ins/VST3/`
- **AU**: `~/Library/Audio/Plug-Ins/Components/`

System-wide install locations if you prefer to install for all users:

- **VST3**: `/Library/Audio/Plug-Ins/VST3/`
- **AU**: `/Library/Audio/Plug-Ins/Components/`

### macOS Gatekeeper

The release builds are not signed or notarized, so macOS or your DAW may still block them in some cases.
`install_mac.sh` already clears extended attributes and attempts ad-hoc signing for user-local installs.

If you install to the system plugin folders manually, start by removing the quarantine flag from the installed bundle:

```bash
sudo xattr -cr "/Library/Audio/Plug-Ins/VST3/NinjamNext.vst3"
```

If your DAW still does not detect the plugin, ad-hoc sign the installed bundle and rescan:

```bash
sudo codesign --force --deep --sign - "/Library/Audio/Plug-Ins/VST3/NinjamNext.vst3"
```

## Local Test Server

The build also produces a local NINJAM server:

```bash
cmake --build build --target ninjamsrv_local
cd ninjam/ninjam/server
../../build/ninjamsrv example.cfg
```

Connect two plugin instances to `localhost:2049` for testing.

## License

NINJAM is licensed under the GPL. See the [ninjam submodule](https://github.com/justinfrankel/ninjam) for details.
