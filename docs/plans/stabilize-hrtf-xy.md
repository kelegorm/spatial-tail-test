# Plan: Stabilize HRTF XY pad movement and remove zebra artifacts

## Overview
Fix unstable binaural behavior during XY pad movement in SpatialTail.
The current HRTF path shows striped tonal/panning artifacts during vertical movement.

Likely causes in current code:
- libmysofa ITD delays (`delayL` / `delayR`) are ignored
- HRTF IRs are replaced abruptly during realtime motion
- azimuth/elevation are not smoothed before HRTF lookup

Goal:
- preserve stable left/right localization while moving in X
- remove obvious zebra-like tonal bands while moving in Y
- keep behavior realtime-safe with no audio-thread heap allocation

## Validation Commands
- `xcodebuild -workspace SpatialTail/SpatialTail.xcworkspace -scheme "macOS-VST3" -configuration Debug build`
- `pluginval --strictness-level 5 ~/Library/Audio/Plug-Ins/VST3/SpatialTail.vst3`

---

### Task 1: Refactor HRTFProcessor state for smooth HRTF transitions
- [x] Inspect `HRTFProcessor` and separate "current active HRTF state" from "newly looked up target HRTF state"
- [x] Add preallocated storage for current and target IRs for both ears
- [x] Add preallocated storage for current and target ITD delays for both ears
- [x] Add transition state needed for short HRTF crossfades without runtime allocation
- [x] Keep fallback behavior unchanged when SOFA is not loaded
- [x] Mark completed

### Task 2: Debug instrumentation for HRTF motion diagnostics
- [x] Add debug-only logging or assertions for azimuth, elevation, distance, selected ITD delays, and transition activity
- [x] Add enough instrumentation to diagnose center collapse, invalid delay values, and transition edge cases
- [x] Add debug flags to independently disable ITD, crossfade, and smoothing for isolated testing
- [x] Keep release builds clean and unchanged
- [x] Mark completed

### Task 3: Automated tests for HRTF processing correctness
- [x] ITD delay test: feed an impulse at a known azimuth, measure arrival time difference between L/R channels, compare against expected `delayL`/`delayR` from SOFA within tolerance
- [x] Crossfade continuity test: change azimuth every block during processing, verify no sample discontinuities between block boundaries (max abs diff between last sample of block N and first sample of block N+1 below threshold)
- [x] Smoothing response test: apply an instantaneous azimuth jump, verify the value reaching the HRTF lookup changes gradually over multiple blocks rather than stepping immediately
- [x] Gain stability test: process white noise through multiple positions (sweep), verify output RMS stays within acceptable bounds and does not clip
- [x] No-allocation test: run processing under debug instrumentation and confirm no heap allocation occurs on the audio thread during position changes
- [x] All tests must pass via validation commands before proceeding to Task 4
- [x] Mark completed

### Task 4: Apply libmysofa ITD delays instead of ignoring them
Note: this task in isolation may sound worse than current behavior because ITD will jump without crossfade smoothing. This is expected and resolved in Task 5.
- [x] Stop discarding `delayL` and `delayR` returned by `mysofa_getfilter_float`
- [x] Implement a small realtime-safe per-ear fractional delay stage suitable for HRTF ITD
- [x] Integrate the delay stage into the HRTF render path in the correct order relative to FIR processing
- [x] Clamp and validate delay values defensively to avoid invalid reads or overruns
- [x] Add clear code comments describing delay units and processing order
- [x] Confirm ITD delay test from Task 3 passes
- [x] Mark completed

### Task 5: Replace abrupt IR switching with short transition smoothing
- [x] Stop hard-switching HRTF filters on each block update
- [x] Implement a short fixed-time transition between previous and target HRTF states
- [x] Ensure the transition covers both FIR response and ITD behavior, not only coefficients
- [x] Keep the implementation deterministic and bounded for fast GUI dragging and automation playback
- [x] Avoid heap allocation, locks, or unbounded work on the audio thread
- [x] Confirm crossfade continuity test and gain stability test from Task 3 pass
- [x] Mark completed

### Task 6: Add azimuth/elevation smoothing before HRTF lookup
Note: smoothing (pre-lookup) and crossfade (post-lookup, Task 5) address different layers of the problem. Smoothing reduces how often the HRTF lookup target jumps; crossfade handles the transition when it does jump. Both are needed for dense SOFA grids.
- [x] Add smoothing state for azimuth and elevation similar in spirit to existing automation smoothing utilities
- [x] Smooth parameter changes before calling into `mHRTF.process(...)`
- [x] Keep existing azimuth sign convention unless a concrete bug is proven
- [x] Reuse existing smoothing helpers/constants where appropriate instead of inventing a separate style
- [x] Verify that distance smoothing and reverb tuning smoothing still behave as before
- [x] Confirm smoothing response test from Task 3 passes
- [x] Confirm all Task 3 tests pass with smoothing and crossfade both enabled
- [x] Mark completed

### Task 7: Build validation and automated test gate
- [ ] Rebuild and run validation commands
- [ ] Run full Task 3 test suite and confirm all tests pass
- [ ] Mark completed

### Task 8: Manual listening validation (blocked on user confirmation)
This task cannot be completed by the agent. After Task 7 passes, prompt the user to perform listening tests and wait for explicit confirmation before marking items complete.
- [ ] User confirms: X movement keeps stable lateral localization
- [ ] User confirms: Y movement no longer produces strong zebra-like tonal striping
- [ ] User confirms: behavior near extreme elevation is acceptable (or documents expected reduced azimuth meaning at poles)
- [ ] Force HRTF load failure in Debug (e.g., invalid SOFA path) and confirm fallback avoids unintended gain boost or clipping
- [ ] If any listening check fails, user describes the issue and agent returns to the relevant task
- [ ] Mark completed
