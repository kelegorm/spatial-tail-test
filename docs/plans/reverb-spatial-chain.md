# Plan: Reverb Before Spatialization

## Overview
Mono input -> mono fold-down -> reverb engine -> HRTF binaural renderer -> stereo output.
Goal: spatialize the reverb output (tail/object), not the raw dry input.

## Validation Commands
- `xcodebuild -workspace SpatialTail/SpatialTail.xcworkspace -scheme "macOS-VST3" -configuration Debug build`
- `pluginval --strictness-level 5 ~/Library/Audio/Plug-Ins/VST3/SpatialTail.vst3`

### Task 1: Reverb processor integration
- [ ] Add/initialize a reverb processor state in `SpatialTail/SpatialTail.h/.cpp` (allocation, reset, sample-rate/block-size handling)
- [ ] Define reverb input contract as mono (single-channel feed from mono fold-down), including explicit channel assumptions in code
- [ ] In `OnReset()` correctly reset reverb internals to avoid stale tail artifacts after transport/sample-rate changes
- [ ] Add safe reverb defaults (room/decay/damping baseline) so routing in Task 2 has predictable behavior
- [ ] Mark completed

### Task 2: DSP routing change (reverb -> HRTF)
- [ ] In `SpatialTail/SpatialTail.cpp` change the processing chain so input is first passed through reverb, and only reverb output is sent to `HRTFProcessor::process()`
- [ ] Keep mono fold-down as the source for the reverb input to preserve a stable spatial object
- [ ] Ensure dry signal path remains controlled by `kDryWet` and does not bypass the new routing by mistake
- [ ] Verify dry tap point is pre-reverb (dry path must come from original mono fold-down/input, not from reverb output)
- [ ] Mark completed

### Task 3: Host contract for latency and tail
- [ ] Determine reverb algorithmic latency; if non-zero, report it to host via plugin latency API and keep dry/wet time-aligned
- [ ] Expose correct tail behavior/length via plugin API so transport stop does not hard-cut reverb tail
- [ ] Rebuild and run validation commands
- [ ] Mark completed

### Task 4: Parameter registration and wiring for reverb stage
- [ ] Add parameters needed for useful reverb behavior (minimum: room/decay and damping/tone control)
- [ ] Register parameters in plugin constructor/parameter layout with explicit ranges and defaults
- [ ] Wire registered parameters in DSP and expose them in GUI controls
- [ ] Verify host automation updates reverb parameters without zipper artifacts
- [ ] Mark completed

### Task 5: Wet-path spatial behavior verification
Listening notes (manual, outside ralphex task checkboxes):
- Confirm by listening test: with `kDryWet=1.0`, moving XY pad changes location of reverberant sound only
- Confirm by listening test: with `kDryWet=0.0`, output is centered dry signal and unaffected by XY pad movement
- [ ] Force HRTF load failure in Debug (e.g., invalid SOFA path) and confirm fallback avoids unintended gain boost or clipping
- [ ] Rebuild and run validation commands
- [ ] Mark completed
