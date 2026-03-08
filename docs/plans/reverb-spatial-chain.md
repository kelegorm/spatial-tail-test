# Plan: Reverb Before Spatialization

## Overview
Mono/stereo input -> mono fold-down -> reverb engine -> HRTF binaural renderer -> stereo output.
Goal: spatialize the reverb output (tail/object), not the raw dry input.

## Validation Commands
- `xcodebuild -workspace SpatialTail/SpatialTail.xcworkspace -scheme "macOS-VST3" -configuration Debug build`
- `pluginval --strictness-level 5 ~/Library/Audio/Plug-Ins/VST3/SpatialTail.vst3`

### Task 1: DSP routing change (reverb -> HRTF)
- [ ] In `SpatialTail/SpatialTail.cpp` change the processing chain so input is first passed through reverb, and only reverb output is sent to `HRTFProcessor::process()`
- [ ] Keep mono fold-down as the source for the reverb input to preserve a stable spatial object
- [ ] Ensure dry signal path remains controlled by `kDryWet` and does not bypass the new routing by mistake
- [ ] Mark completed

### Task 2: Reverb processor integration
- [ ] Add/initialize a reverb processor state in `SpatialTail/SpatialTail.h/.cpp` (allocation, reset, sample-rate/block-size handling)
- [ ] In `OnReset()` correctly reset reverb internals to avoid stale tail artifacts after transport/sample-rate changes
- [ ] Add safe defaults so plugin startup produces audible wet signal before user tweaks
- [ ] Mark completed

### Task 3: Parameter wiring for reverb stage
- [ ] Add parameters needed for useful reverb behavior (minimum: room/decay and damping/tone control)
- [ ] Wire parameters in DSP and expose them in GUI controls
- [ ] Verify host automation updates reverb parameters without zipper artifacts
- [ ] Mark completed

### Task 4: Wet-path spatial behavior verification
- [ ] Confirm by listening test: with `kDryWet=1.0`, moving XY pad changes location of reverberant sound only
- [ ] Confirm by listening test: with `kDryWet=0.0`, output is centered dry signal and unaffected by XY pad movement
- [ ] Confirm fallback behavior when HRTF is unavailable still avoids unintended gain boost or clipping
- [ ] Rebuild and run validation commands
- [ ] Mark completed
