# D.A Room — Algorithmic Reverb Plugin (VST3 & AU)

**D.A Room** by **D.A Production** is a C++17 / JUCE 8.0.6 stereo algorithmic reverb plugin featuring a custom high-DPI interface (`2030 × 775` reference canvas), 50 categorized factory presets, A/B state comparison, `.drpreset` file import/export, and a custom frameless Windows x64 `.exe` installer.

## Features
- **Formats**:
  - **Windows 64-bit VST3** (`D.A Room.vst3`, static MSVC runtime — zero external DLL dependencies)
  - **macOS Universal 2 AU & VST3** (`D.A Room.component` & `D.A Room.vst3` for Apple Silicon `arm64` + Intel `x86_64`)
  - **Custom Windows x64 `.EXE` Installer** (`D.A Production Installer.exe` installing to `C:\Program Files\Common Files\VST3`)
- **DSP Architecture**:
  - Click-free smoothed fractional-delay Predelay (`0–250 ms`)
  - 4-stage stereo prime-ratio Schroeder allpass input diffuser (`Diffusion`)
  - 5 acoustic space voicings (`Room`, `Hall`, `Plate`, `Chamber`, `Ambience`) with perceptual `Decay` (`0.20–12.00 s`) and `Size` (`10–100%`)
  - Quadrature 90° stereo chorus modulation (`Modulation`)
  - Zero-allocation TPT 12 dB/oct Low Cut (`20–1000 Hz`) & High Cut (`2–20 kHz`) filters
  - Smoothed M/S Stereo Width (`0–100%`) and equal-power Wet/Dry Mix (`0–100%`)
  - Full `Mono -> Mono`, `Mono -> Stereo`, and `Stereo -> Stereo` bus support
- **Presets & Workflow**:
  - 50 factory presets across Room, Hall, Plate, Chamber, and Ambience categories
  - Persistent A/B state comparison & `.drpreset` JSON preset load/save

## Build Locally

```bash
# macOS (Universal AU + VST3)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target DAroom_AU DAroom_VST3

# Windows x64 (VST3 + Custom EXE Installer)
cmake -S . -B build -A x64
cmake --build build --config Release --target DAroom_VST3 DA_Production_Installer
```
