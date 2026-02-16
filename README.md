# NINJAM Next Plugin

A modern JUCE-based VST3/AU plugin for [NINJAM](https://www.cockos.com/ninjam/) — collaborative real-time music jamming over the internet, right inside your DAW.

## Features

- VST3 (Windows + macOS) and AU (macOS) plugin formats
- Host transport sync with ring-buffer phase alignment
- Classic NINJAM mode with auto-subscribe
- Built-in metronome aligned to DAW beats
- Local/remote gain controls
- Chat and log panel

## Build

### Prerequisites

- CMake 3.22+
- **Windows**: Visual Studio 2022+ with C++ workload
- **macOS**: Xcode command line tools

### Clone & Build

```bash
git clone --recursive https://github.com/nykwil/ninjam-next-plugin.git
cd ninjam-next-plugin
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target NinjamVST3_VST3
```

On Windows you can also use the one-click script:

```powershell
build_win.bat
```

### Output

- **VST3**: `build/NinjamVST3_artefacts/<config>/VST3/NINJAM VST3.vst3/`
- **AU** (macOS): `build/NinjamVST3_artefacts/<config>/AU/`

Copy the plugin bundle into your system's VST3 (or AU Components) directory, then rescan in your DAW.

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
