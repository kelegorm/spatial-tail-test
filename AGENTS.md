# AGENTS.md

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

Run standalone reverb-stage regression tests (CMake/CTest path):
```bash
cmake -S SpatialTail -B SpatialTail/build
cmake --build SpatialTail/build --target reverb_stage_tests
ctest --test-dir SpatialTail/build -R reverb_stage_tests --output-on-failure
```

Debug-only HRTF failure injection:
```bash
export SPATIALTAIL_DEBUG_FORCE_HRTF_LOAD_FAILURE=1
```

## Architecture
Signal flow: host input -> mono fold-down -> reverb engine -> wet mono -> HRTFProcessor::process() -> stereo wet -> dry/wet mix -> stereo output.

- HRTFProcessor (SpatialTail/HRTFProcessor.h/.cpp): wraps libmysofa MYSOFA_EASY, loads a SOFA file on OnReset(), performs sample-by-sample time-domain FIR convolution using a circular input buffer (size = filter length)
- Reverb stage (SpatialTail/ReverbStage.h): owns reverb defaults, runtime room/damping apply, host latency/tail probing, mono/stereo contracts, and dry-alignment delay helper
- SOFA file path: controlled by DEFAULT_SOFA_PATH macro (SpatialTail.cpp); currently an absolute dev path, prototype only
- Parameters: kAzimuth (-90..90 deg), kElevation (-90..90 deg), kDistance (0.1..10 m), kDryWet (0..1), kReverbRoomSize (0.0..0.99), kReverbDamping (0.0..1.0)
- Channel IO: "1-2" (mono in, stereo out)
- Host contract: plugin reports realtime-safe estimated latency (`SetLatency`) and conservative finite tail length (`SetTailSize`)
- ITD delays from libmysofa are read but intentionally not applied (MVP limitation)

## Review Automation

- `.ralphex/scripts/Codex-external-review.sh` wraps `Codex -p` for external review calls and requires a valid prompt-file argument.

## Conventions
- One task at a time, commit after each completed task
- Always run build before marking task done
- Never mark task complete if build fails
