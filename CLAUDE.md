# CLAUDE.md

## Project
Binaural reverb tail prototype VST3/CLAP plugin.
Goal: test one reverb tail wrapped as a spatially localized 3D object using binaural panning (HRTF).
This is a proof-of-concept before building the full "cloudverb" effect.

## Stack
- C++17
- iPlug2 framework (VST3 + CLAP + AU from single codebase)
- HRTF-based binaural panning
- libmysofa (SOFA/HRTF file loading, git submodule at libs/libmysofa)
- Target DAW: Bitwig Studio on macOS

## Build

Build VST3 (Debug):
```bash
xcodebuild -workspace SpatialTail/SpatialTail.xcworkspace \
  -scheme "macOS-VST3" \
  -configuration Debug \
  build
```

Install to `~/Library/Audio/Plug-Ins/VST3/` happens automatically as part of the build.
If it fails with a permissions error on that directory, fix once with:
```bash
sudo chown -R $(whoami) ~/Library/Audio/Plug-Ins/VST3
```

One-time: build libmysofa static library before first Xcode build:
```bash
cd libs/libmysofa
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
cmake --build build
cd ../..
```

## Test
```bash
pluginval --strictness-level 5 ~/Library/Audio/Plug-Ins/VST3/SpatialTail.vst3
```

## Architecture
Signal flow: mono input → HRTFProcessor::process() → stereo L/R FIR convolution → dry/wet mix → stereo output.

- HRTFProcessor (SpatialTail/HRTFProcessor.h/.cpp): wraps libmysofa MYSOFA_EASY, loads a SOFA file on OnReset(), performs sample-by-sample time-domain FIR convolution using a circular input buffer (size = filter length)
- SOFA file path: controlled by DEFAULT_SOFA_PATH macro (SpatialTail.cpp); currently an absolute dev path, prototype only
- Parameters: kAzimuth (-180..180 deg), kElevation (-90..90 deg), kDistance (0.1..10 m), kDryWet (0..1)
- Channel IO: "1-2" (mono in, stereo out)
- ITD delays from libmysofa are read but intentionally not applied (MVP limitation)

## Conventions
- One task at a time, commit after each completed task
- Always run build before marking task done
- Never mark task complete if build fails
