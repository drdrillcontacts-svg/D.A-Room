# DAroom — VST3 / AU starter project

DAroom is an original JUCE-based algorithmic reverb plugin starter with the selected 653 × 184 px GUI reference embedded in the build. The interface uses the supplied artwork as its background and places interactive parameter controls and a 36-preset browser over the corresponding hit areas.

## Included
- C++17 + JUCE 8.0.6 project
- VST3, AU (macOS), and Standalone formats
- Mix, Predelay, Decay, Size and reverb mode parameters
- Parameter state save/restore
- 36 factory presets across room, hall, plate, chamber and ambience spaces
- Individual editable `.drpreset` preset files in `Presets/`
- GitHub Actions workflow for macOS and Windows builds
- Exact GUI reference at `Assets/DAroom_UI_Reference.png`

## Important quality note
This is a functional first implementation, not a claim of parity with ValhallaRoom. It uses JUCE's built-in stereo reverb as a starting point plus a predelay buffer. Matching a mature commercial reverb requires substantial DSP design, listening tests, CPU optimization, parameter smoothing, and validation across sample rates and DAWs.

## Build locally
Install CMake, a C++ toolchain, and Git. Then:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

On macOS, CMake builds VST3 and AU targets. AU is only available on Apple platforms.

## GitHub Actions
Push the project to GitHub. The workflow builds on macOS and Windows and uploads artifacts. For macOS AU validation, use a Mac runner; a Windows runner cannot build AU.
