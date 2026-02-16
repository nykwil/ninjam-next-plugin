# NINJAM VST3 Plugin

A JUCE-based VST3/AU plugin that wraps the legacy NINJAM client (`NJClient`) for use inside a DAW. Cross-platform (Windows + macOS), built with CMake.

## Project Structure

```
ninjam-next-plugin/
├── CMakeLists.txt          # CMake build config (fetches JUCE 8.0.3, libogg, libvorbis)
├── build_win.bat           # One-click Windows build (Ninja + RelWithDebInfo)
├── .github/workflows/      # CI: macOS + Windows builds
├── src/
│   ├── NinjamClientService.h/cpp   # Core NINJAM wrapper (audio, sync, ring buffers, metronome)
│   ├── PluginProcessor.h/cpp       # JUCE AudioProcessor (transport, state persistence)
│   └── PluginEditor.h/cpp          # JUCE UI (connection, controls, log panel)
├── ninjam/                 # git submodule → justinfrankel/ninjam
│   ├── ninjam/             # legacy NJClient C++ source
│   └── WDL/                # Cockos WDL library (jnetlib, SHA, RNG)
├── build/                  # Build output (gitignored)
└── CLAUDE.md               # This file
```

## Building

### Prerequisites
- CMake 3.22+
- **Windows**: Visual Studio 2022+ (or VS Build Tools) with C++ workload, Ninja
- **macOS**: Xcode command line tools

### Clone
```
git clone --recursive https://github.com/nykwil/ninjam-next-plugin.git
cd ninjam-next-plugin
```

### Quick Build (Windows)
```
build_win.bat
```

### Manual Build
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target NinjamVST3_VST3
```

### Build Output
- VST3 plugin: `build/NinjamVST3_artefacts/<config>/VST3/NINJAM VST3.vst3/`
- AU plugin (macOS only): `build/NinjamVST3_artefacts/<config>/AU/`
- Local test server: `cmake --build build --target ninjamsrv_local`

## Architecture

### Audio Flow (host-locked mode)

```
DAW input buffer
  → inputScratch (apply local gain)
  → [INPUT RING: write at DAW-beat pos, read at server pos] (sender phase alignment)
  → NJClient::AudioProc()
  → outputScratch
  → [OUTPUT RING: write at server pos, read at DAW-beat pos] (receiver phase alignment)
  → plugin-side metronome (sine clicks aligned to DAW beats)
  → DAW output buffer (apply remote gain)
```

### Sync Modes
- **Host Locked** — DAW transport is playing with valid clock. Ring buffers active, plugin renders its own metronome.
- **Fallback (Host Stopped)** — DAW transport stopped. NJClient runs freely, uses its own metronome.
- **Fallback (No Host Clock)** — No playhead available (standalone host). Same as above.

### Ring Buffer Calibration
- Calibrated once at the first NINJAM interval boundary after connect or BPM/BPI change
- Records the DAW's PPQ phase at that moment (`phaseRingBeatOffset`)
- `alignedBeat = fmod(rawDawPhase - phaseRingBeatOffset, BPI)`
- Ensures DAW beat 0 ↔ server interval position 0 for both sender and receiver
- Reset on disconnect or when server changes BPM/BPI

### Key Classes
- **`NinjamClientService`** — owns `NJClient`, handles audio processing, ring buffers, metronome, chat, connection state. Thread-safe via `juce::CriticalSection`.
- **`NinjamVST3AudioProcessor`** — JUCE `AudioProcessor`. Builds `TransportState` from host playhead, owns `NinjamClientService`, handles state save/restore.
- **`NinjamVST3AudioProcessorEditor`** — JUCE UI. Polls `Snapshot` at 10 Hz for display updates.

### Threading Model
- Audio thread: calls `processAudioBlock()` — does ring buffer I/O, calls `NJClient::AudioProc()`
- Timer thread (20 Hz): calls `NJClient::Run()` (networking), `refreshStatusFromCore()`
- UI thread: reads `Snapshot` (thread-safe copy), writes settings via setters
- All shared state protected by `juce::CriticalSection lock`

## Local Test Server

```
cd ninjam/ninjam/server
../../build/ninjamsrv example.cfg
```

Default config runs on port 2049. Connect two plugin instances to `localhost:2049` for testing.

## NJClient Integration Notes

- Uses **classic mode** only (session mode flags `2|4` are cleared in `applySessionChannelModeToCore()`)
- `config_session_monitor_latest = false` — prevents session-mode latest-interval monitoring
- `config_autosubscribe = 1` — auto-subscribes to remote channels
- `config_savelocalaudio = 0` — doesn't save local audio to disk
- `config_play_prebuffer = 4096` — prebuffer size for playback
- NJClient's built-in metronome is muted when host-locked; plugin renders its own phase-aligned metronome
- License agreements are auto-accepted

## Data Paths
- Settings: `%APPDATA%\Nykwil\NinjamVST3.settings` (XML) / `~/Library/Application Support/Nykwil/` (macOS)
- Session data: `%APPDATA%\Nykwil\NinjamVST3\sessions\`
- Log file: `%APPDATA%\Nykwil\NinjamVST3\ninjam-client.log`
