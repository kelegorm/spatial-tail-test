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
- [ ] Create SpatialTail/HRTFProcessor.h with class definition
- [ ] Create SpatialTail/HRTFProcessor.cpp
- [ ] Implement constructor: load default.sofa bundled with libmysofa via mysofa_load()
- [ ] Implement process(float* in, float* outL, float* outR, int nFrames, float azimuth, float elevation, float distance): find nearest HRIR via mysofa_getfilter_float(), apply time-domain FIR convolution
- [ ] Add HRTFProcessor.cpp to SpatialTail-macOS.xcodeproj sources
- [ ] Mark completed

### Task 4: Wire DSP
- [ ] In SpatialTail/SpatialTail.h add HRTFProcessor member and parameters for azimuth, elevation, distance, dryWet
- [ ] In SpatialTail/SpatialTail.cpp constructor initialize HRTFProcessor with default.sofa path
- [ ] In ProcessBlock: sum stereo input to mono, call HRTFProcessor::process(), mix with dry via dryWet
- [ ] Mark completed

### Task 5: GUI
- [ ] Add XY pad control for Azimuth (X axis -180..180) and Elevation (Y axis -90..90)
- [ ] Add knob for Distance
- [ ] Add knob for DryWet
- [ ] Connect controls to plugin parameters
- [ ] Mark completed