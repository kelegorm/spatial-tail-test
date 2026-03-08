# Reverb Room/Damping Regression Capture

Date: 2026-03-08
Scope: Task 6 objective reproduction for wet-only output behavior.

## Test setup
- Engine: `WDL_ReverbEngine` via `SpatialTail/ReverbStage.h`
- Sample rate: 48 kHz
- Block size: 256
- Duration: 120 blocks
- Input signals:
  - Impulse (sample 0 = 1.0, remaining 0)
  - Deterministic pink noise (fixed seed)
- Wet-only metrics:
  - RMS
  - High-frequency ratio (`diffEnergy / energy`)
  - Envelope mean (windowed absolute amplitude)

## Captured behavior before fix path
Tuning with only `SetRoomSize`/`SetDampening` (no `Reset(false)` afterward):

- Impulse `Room=0.0` vs `Room=0.99`: RMS delta 0.000000000, HF delta 0.000000000, Envelope delta 0.000000000
- Impulse `Damping=0.0` vs `Damping=1.0`: RMS delta 0.000000000, HF delta 0.000000000, Envelope delta 0.000000000
- Pink noise `Room=0.0` vs `Room=0.99`: RMS delta 0.000000000, HF delta 0.000000000, Envelope delta 0.000000000
- Pink noise `Damping=0.0` vs `Damping=1.0`: RMS delta 0.000000000, HF delta 0.000000000, Envelope delta 0.000000000

Result: Room and damping changes were objectively not reaching the wet output.

## Root cause
`WDL_ReverbEngine` requires `Reset(false)` after room/damping changes to latch coefficients into comb filters.
The old path set target values but skipped that reset, so effective processing stayed unchanged.

## Captured behavior with fixed runtime apply path
Tuning with runtime helper that applies room/damping and calls `Reset(false)` on change:

- Impulse `Room=0.0` vs `Room=0.99`: RMS delta 0.003579341, HF delta 0.888617969, Envelope delta 0.440012257
- Impulse `Damping=0.0` vs `Damping=1.0`: RMS delta 0.000693261, HF delta 0.264448684, Envelope delta 0.092247082
- Pink noise `Room=0.0` vs `Room=0.99`: RMS delta 0.091516659, HF delta 0.011488390, Envelope delta 5.658448507
- Pink noise `Damping=0.0` vs `Damping=1.0`: RMS delta 0.003802889, HF delta 0.098408289, Envelope delta 0.249622440

Result: both controls now produce measurable wet-output changes.
