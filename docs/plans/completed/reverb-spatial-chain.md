# Plan: Reverb Before Spatialization

## Overview
Mono input -> mono fold-down -> reverb engine -> HRTF binaural renderer -> stereo output.
Goal: spatialize the reverb output (tail/object), not the raw dry input.

## Validation Commands
- `xcodebuild -workspace SpatialTail/SpatialTail.xcworkspace -scheme "macOS-VST3" -configuration Debug build`
- `pluginval --strictness-level 5 ~/Library/Audio/Plug-Ins/VST3/SpatialTail.vst3`

### Task 1: Reverb processor integration
- [x] Add/initialize a reverb processor state in `SpatialTail/SpatialTail.h/.cpp` (allocation, reset, sample-rate/block-size handling)
- [x] Define reverb input contract as mono (single-channel feed from mono fold-down), including explicit channel assumptions in code
- [x] In `OnReset()` correctly reset reverb internals to avoid stale tail artifacts after transport/sample-rate changes
- [x] Add safe reverb defaults (room/decay/damping baseline) so routing in Task 2 has predictable behavior
- [x] Mark completed

### Task 2: DSP routing change (reverb -> HRTF)
- [x] In `SpatialTail/SpatialTail.cpp` change the processing chain so input is first passed through reverb, and only reverb output is sent to `HRTFProcessor::process()`
- [x] Keep mono fold-down as the source for the reverb input to preserve a stable spatial object
- [x] Ensure dry signal path remains controlled by `kDryWet` and does not bypass the new routing by mistake
- [x] Verify dry tap point is pre-reverb (dry path must come from original mono fold-down/input, not from reverb output)
- [x] Mark completed

### Task 3: Host contract for latency and tail
- [x] Determine reverb algorithmic latency; if non-zero, report it to host via plugin latency API and keep dry/wet time-aligned
- [x] Expose correct tail behavior/length via plugin API so transport stop does not hard-cut reverb tail
- [x] Rebuild and run validation commands
- [x] Mark completed

### Task 4: Parameter registration and wiring for reverb stage
- [x] Add parameters needed for useful reverb behavior (minimum: room/decay and damping/tone control)
- [x] Register parameters in plugin constructor/parameter layout with explicit ranges and defaults
- [x] Wire registered parameters in DSP and expose them in GUI controls
- [x] Verify host automation updates reverb parameters without zipper artifacts
- [x] Mark completed

### Task 5: Wet-path spatial behavior verification
Listening notes (manual, outside ralphex task checkboxes):
- Confirm by listening test: with `kDryWet=1.0`, moving XY pad changes location of reverberant sound only
- Confirm by listening test: with `kDryWet=0.0`, output is centered dry signal and unaffected by XY pad movement
- [x] Force HRTF load failure in Debug (e.g., invalid SOFA path) and confirm fallback avoids unintended gain boost or clipping
- [x] Rebuild and run validation commands
- [x] Mark completed

### Task 6: Fix regression where Room/Damping knobs do not audibly change wet signal
- [x] Reproduce and document the bug with an objective case (impulse and pink-noise input): capture wet-only output for `Room=0.0` vs `Room=0.99` and `Damping=0.0` vs `Damping=1.0`
- [x] Audit parameter flow end-to-end (`GetParam` -> smoothing -> `ApplyReverbTuning` -> `WDL_ReverbEngine` processing) and identify where values are lost or overridden
- [x] Ensure reverb tuning updates are applied at runtime without accidental reset/default re-application in `OnReset()` or per-block processing
- [x] Add/extend `SpatialTail/tests/reverb_stage_tests.cpp` so tests fail if changing `Room` or `Damping` does not measurably change wet output metrics (energy envelope / high-frequency content)
- [x] Add a fast debug assertion/log guard (Debug-only) that verifies reverb engine receives changed room/damping values when host automation moves knobs
- [x] Rebuild and run validation commands plus reverb-stage tests
- [x] Manual listening verification in host: with `kDryWet=1.0`, both knobs produce clearly audible tail/tone changes across full range
- [x] Mark completed
