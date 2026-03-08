# CLAUDE.md

## Project
Binaural reverb tail prototype VST3/CLAP plugin.
Goal: test one reverb tail wrapped as a spatially localized 3D object using binaural panning (HRTF).
This is a proof-of-concept before building the full "cloudverb" effect.

## Stack
- C++17
- iPlug2 framework (VST3 + CLAP + AU from single codebase)
- HRTF-based binaural panning
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

## Test
(fill in after iPlug2 setup)

## Architecture
(fill in after iPlug2 setup)

## Conventions
- One task at a time, commit after each completed task
- Always run build before marking task done
- Never mark task complete if build fails
