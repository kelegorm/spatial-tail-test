# Plan: Binaural Core MVP

## Overview
Mono input → HRTF binaural renderer → stereo output.
Spatially localized reverb tail as a 3D object in headphone space.
Stack: iPlug2, libmysofa (with bundled default.sofa), C++17, Xcode.

## Validation Commands
- `xcodebuild -workspace SpatialTail/SpatialTail.xcworkspace -scheme "macOS-VST3" -configuration Debug build`
- `pluginval --strictness-level 5 ~/Library/Audio/Plug-Ins/VST3/SpatialTail.vst3`

### Task 1: Channel IO
- [x] In SpatialTail/config/config.h change PLUG_CHANNEL_IO to "1-2" (mono in, stereo out)
- [x] Rebuild and verify plugin still loads
- [x] Mark completed

### Task 2: libmysofa dependency
- [x] Add libmysofa as git submodule in libs/libmysofa using https://github.com/hoene/libmysofa
- [x] Build libmysofa as static library using CMake into libs/libmysofa/build/
- [x] Add libs/libmysofa/src to Header Search Paths in SpatialTail-macOS.xcodeproj
- [x] Add libs/libmysofa/build/src/libmysofa.a to Link Binary With Libraries in SpatialTail-macOS.xcodeproj
- [x] Add zlib (system) to Link Binary With Libraries
- [x] Verify build succeeds with libmysofa linked
- [x] Mark completed

### Task 3: HRTFProcessor class
- [x] Create SpatialTail/HRTFProcessor.h with class definition
- [x] Create SpatialTail/HRTFProcessor.cpp
- [x] Implement constructor: load default.sofa bundled with libmysofa via mysofa_load()
- [x] Implement process(float* in, float* outL, float* outR, int nFrames, float azimuth, float elevation, float distance): find nearest HRIR via mysofa_getfilter_float(), apply time-domain FIR convolution
- [x] Add HRTFProcessor.cpp to SpatialTail-macOS.xcodeproj sources
- [x] Mark completed

### Task 4: Wire DSP
- [x] In SpatialTail/SpatialTail.h add HRTFProcessor member and parameters for azimuth, elevation, distance, dryWet
- [x] In SpatialTail/SpatialTail.cpp constructor initialize HRTFProcessor with default.sofa path
- [x] In ProcessBlock: sum stereo input to mono, call HRTFProcessor::process(), mix with dry via dryWet
- [x] Mark completed

### Task 5: GUI
- [x] Add XY pad control for Azimuth (X axis -180..180) and Elevation (Y axis -90..90)
- [x] Add knob for Distance
- [x] Add knob for DryWet
- [x] Connect controls to plugin parameters
- [x] Mark completed

### Task 6: Distance parameter audibility fix
- [x] Confirm current behavior: `kDistance` is only passed to `mysofa_getfilter_float()` and does not apply gain/air attenuation in DSP
- [x] In SpatialTail/SpatialTail.cpp add an explicit distance model after HRTF render (minimum: inverse-distance gain with safe clamp)
- [x] Add optional smoothing for distance changes to avoid zipper noise
- [x] Keep HRTF lookup distance argument, but make loudness change guaranteed across full knob travel
- [x] Rebuild and verify audible change at 0.1 m vs 10 m on the same source position
- [x] Mark completed

### Task 7: XY left-right direction fix
- [x] Fix azimuth sign convention mismatch so moving XY point right produces sound from listener right
- [x] Implement mapping in one place only (recommended: inside HRTFProcessor::updateFilter or before calling it), avoid double inversion
- [x] Add a quick manual check procedure: center (0), right edge (+X), left edge (-X)
- [x] Rebuild and verify mapping in DAW headphones test
- [x] Mark completed

### Task 8: Front hemisphere limit for XY pad
- [x] Change azimuth control range from `[-180, 180]` to `[-90, 90]` to prevent routing behind the head
- [x] Ensure XY pad X axis label and parameter metadata reflect the new range
- [x] Clamp azimuth/elevation in DSP path before SOFA lookup to enforce bounds regardless of host automation values
- [x] Keep elevation limited to front-hemisphere-friendly range so top/bottom movement does not place source behind
- [x] Rebuild and verify extreme XY edges correspond to ±90 deg, not rear positions
- [x] Mark completed
