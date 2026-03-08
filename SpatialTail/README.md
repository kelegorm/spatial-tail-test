# SpatialTail

SpatialTail is a mono-in/stereo-out reverb spatializer prototype built with iPlug2.
It spatializes only the wet reverb tail through HRTF, while keeping a dry mono path.

## Signal Flow

- Host input channels are folded down to one mono feed.
- Mono feed is processed by `WDL_ReverbEngine`.
- Reverb wet output is collapsed to mono and rendered through `HRTFProcessor` to stereo.
- Dry path is time-aligned to reported reverb latency before dry/wet mixing.
- If HRTF fails to load, wet output falls back to clamped dual-mono without distance boost.

## Parameters

- Azimuth: `-90..90` deg, default `0`
- Elevation: `-90..90` deg, default `0`
- Distance: `0.1..10` m, default `1.0`
- Dry/Wet: `0..1`, default `1.0`
- Reverb Room: `0.0..0.99`, default `0.72`
- Reverb Damping: `0.0..1.0`, default `0.30`
