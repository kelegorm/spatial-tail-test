#include "SpatialTail.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"
#include <cmath>

// Path to the SOFA file used for development (absolute path, prototype only)
#ifndef DEFAULT_SOFA_PATH
#define DEFAULT_SOFA_PATH "/Users/dmitry/Work/my_own/spatial-tail-test/libs/libmysofa/share/MIT_KEMAR_normal_pinna.sofa"
#endif

SpatialTail::SpatialTail(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kAzimuth)->InitDouble("Azimuth", 0., -90., 90., 1., "deg");
  GetParam(kElevation)->InitDouble("Elevation", 0., -90., 90., 1., "deg");
  GetParam(kDistance)->InitDouble("Distance", 1., 0.1, 10., 0.01, "m");
  GetParam(kDryWet)->InitDouble("Dry/Wet", 1., 0., 1., 0.01, "");
  GetParam(kReverbRoomSize)->InitDouble("Reverb Room", spatialtail::kReverbDefaultRoomSize,
                                        spatialtail::kReverbRoomMin, spatialtail::kReverbRoomMax, 0.001, "");
  GetParam(kReverbDamping)->InitDouble("Reverb Damping", spatialtail::kReverbDefaultDamping,
                                       spatialtail::kReverbDampingMin, spatialtail::kReverbDampingMax, 0.001, "");

  const auto initialTiming = spatialtail::MeasureReverbHostTiming(44100.0);
  mReverbLatencySamples = initialTiming.latencySamples;
  mReverbTailSamples = initialTiming.tailSamples;
  SetLatency(mReverbLatencySamples);
  SetTailSize(mReverbTailSamples);

#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(COLOR_GRAY);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    const IRECT bounds = pGraphics->GetBounds();
    const IRECT innerBounds = bounds.GetPadded(-10.f);

    // Version info top-right
    const IRECT versionBounds = innerBounds.GetFromTRHC(300, 20);
    WDL_String buildInfoStr;
    GetBuildInfoStr(buildInfoStr, __DATE__, __TIME__);
    pGraphics->AttachControl(new ITextControl(versionBounds, buildInfoStr.Get(), DEFAULT_TEXT.WithAlign(EAlign::Far)));

    // Title at top
    const IRECT titleBounds = innerBounds.GetFromTop(40.f);
    pGraphics->AttachControl(new ITextControl(titleBounds, "SpatialTail", IText(28)));

    // XY pad for Azimuth (X) and Elevation (Y) - large square in upper area
    const float padSize = 380.f;
    const float padTop = 55.f;
    const IRECT padBounds = IRECT::MakeMidXYWH(bounds.MW(), padTop + padSize * 0.5f, padSize, padSize);
    pGraphics->AttachControl(new IVXYPadControl(padBounds, {kAzimuth, kElevation}, "Azimuth / Elevation"));

    // Labels for XY axes
    const IRECT xLabelBounds = IRECT(padBounds.L, padBounds.B + 2.f, padBounds.R, padBounds.B + 18.f);
    pGraphics->AttachControl(new ITextControl(xLabelBounds, "Azimuth (-90..90 deg, front hemisphere)", IText(12)));

    // Knobs row at the bottom
    const float knobSize = 72.f;
    const float knobSpacing = 28.f;
    const float knobY = padBounds.B + 30.f;
    const float totalKnobWidth = knobSize * 4 + knobSpacing * 3;
    const float knobStartX = bounds.MW() - totalKnobWidth * 0.5f;

    const IRECT roomBounds = IRECT(knobStartX, knobY, knobStartX + knobSize, knobY + knobSize);
    pGraphics->AttachControl(new IVKnobControl(roomBounds, kReverbRoomSize, "Room"));

    const IRECT dampingBounds = IRECT(knobStartX + knobSize + knobSpacing, knobY,
                                      knobStartX + knobSize * 2 + knobSpacing, knobY + knobSize);
    pGraphics->AttachControl(new IVKnobControl(dampingBounds, kReverbDamping, "Damping"));

    const IRECT distanceBounds = IRECT(knobStartX + (knobSize + knobSpacing) * 2, knobY,
                                       knobStartX + (knobSize + knobSpacing) * 2 + knobSize, knobY + knobSize);
    pGraphics->AttachControl(new IVKnobControl(distanceBounds, kDistance, "Distance"));

    const IRECT dryWetBounds = IRECT(knobStartX + (knobSize + knobSpacing) * 3, knobY,
                                     knobStartX + (knobSize + knobSpacing) * 3 + knobSize, knobY + knobSize);
    pGraphics->AttachControl(new IVKnobControl(dryWetBounds, kDryWet, "Dry/Wet"));
  };
#endif
}

#if IPLUG_DSP
void SpatialTail::OnReset()
{
  // Only reload SOFA (disk I/O) when the sample rate actually changes.
  // Buffer-size-only resets still resize the scratch buffers below.
  const double sr = GetSampleRate();
  spatialtail::ResetReverb(mReverb, sr);
  mSmoothedReverbRoomSize = static_cast<float>(GetParam(kReverbRoomSize)->Value());
  mSmoothedReverbDamping = static_cast<float>(GetParam(kReverbDamping)->Value());
  spatialtail::ApplyReverbTuning(mReverb, mSmoothedReverbRoomSize, mSmoothedReverbDamping);

  if (sr != mLastSampleRate)
  {
    mLastSampleRate = sr;
    const auto reverbTiming = spatialtail::MeasureReverbHostTiming(sr);
    mReverbLatencySamples = reverbTiming.latencySamples;
    mReverbTailSamples = reverbTiming.tailSamples;
    SetLatency(mReverbLatencySamples);
    SetTailSize(mReverbTailSamples);

    const char* sofaPath = spatialtail::ResolveHRTFLoadPath(DEFAULT_SOFA_PATH);
    if (!mHRTF.load(sofaPath, static_cast<float>(sr)))
      DBGMSG("HRTFProcessor: failed to load SOFA file: %s\n", sofaPath);

    // One-pole smoother: time constant ~20 ms to avoid zipper noise on distance knob
    mDistanceSmoothCoeff = 1.f - std::exp(-1.f / (0.020f * static_cast<float>(sr)));
    mSmoothedDistanceGain = 1.f; // reset to reference distance gain
  }

  const int blockSize = GetBlockSize();
  mMonoIn.assign(blockSize, 0.f);
  mReverbInL.assign(blockSize, 0.0);
  mReverbInR.assign(blockSize, 0.0);
  mReverbOutL.assign(blockSize, 0.0);
  mReverbOutR.assign(blockSize, 0.0);
  mDryAlignedMono.assign(blockSize, 0.f);
  mReverbWetMono.assign(blockSize, 0.f);
  mHrtfL.assign(blockSize, 0.f);
  mHrtfR.assign(blockSize, 0.f);
  spatialtail::PrepareMonoDelayLine(mDryDelayLine, mReverbLatencySamples, mDryDelayWritePos);
}

void SpatialTail::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const float azimuth  = static_cast<float>(GetParam(kAzimuth)->Value());
  const float elevation = static_cast<float>(GetParam(kElevation)->Value());
  const float distance = static_cast<float>(GetParam(kDistance)->Value());
  const float dryWet   = static_cast<float>(GetParam(kDryWet)->Value());
  const float reverbRoomTarget = static_cast<float>(GetParam(kReverbRoomSize)->Value());
  const float reverbDampingTarget = static_cast<float>(GetParam(kReverbDamping)->Value());

  if (nFrames > static_cast<int>(mMonoIn.size()))
  {
    mMonoIn.assign(nFrames, 0.f);
    mReverbInL.assign(nFrames, 0.0);
    mReverbInR.assign(nFrames, 0.0);
    mReverbOutL.assign(nFrames, 0.0);
    mReverbOutR.assign(nFrames, 0.0);
    mDryAlignedMono.assign(nFrames, 0.f);
    mReverbWetMono.assign(nFrames, 0.f);
    mHrtfL.assign(nFrames, 0.f);
    mHrtfR.assign(nFrames, 0.f);
  }

  // Reverb input contract: the reverb stage takes exactly one mono feed.
  // The source is a mono fold-down of host inputs and is then duplicated to
  // dual-mono for the current reverb engine implementation.
  const int nInChans = NInChansConnected();
  spatialtail::FoldDownToMono(inputs, nInChans, mMonoIn.data(), nFrames);

  const float reverbSmoothCoeff = spatialtail::ComputeBlockSmoothingCoefficient(
      GetSampleRate(), nFrames, spatialtail::kAutomationSmoothingTimeSeconds);
  mSmoothedReverbRoomSize = spatialtail::SmoothTowards(mSmoothedReverbRoomSize, reverbRoomTarget, reverbSmoothCoeff);
  mSmoothedReverbDamping = spatialtail::SmoothTowards(mSmoothedReverbDamping, reverbDampingTarget, reverbSmoothCoeff);
  spatialtail::ApplyReverbTuning(mReverb, mSmoothedReverbRoomSize, mSmoothedReverbDamping);

  spatialtail::CopyMonoToStereoReverbInputs(mMonoIn.data(), mReverbInL.data(), mReverbInR.data(), nFrames);
  mReverb.ProcessSampleBlock(mReverbInL.data(), mReverbInR.data(), mReverbOutL.data(), mReverbOutR.data(), nFrames);
  spatialtail::CollapseStereoReverbToMono(mReverbOutL.data(), mReverbOutR.data(), mReverbWetMono.data(), nFrames);
  spatialtail::ApplyMonoDelay(mMonoIn.data(), mDryAlignedMono.data(), nFrames, mReverbLatencySamples, mDryDelayLine, mDryDelayWritePos);

  const bool hrtfLoaded = mHRTF.isLoaded();
  if (hrtfLoaded)
  {
    // Spatialize only the reverb output to keep dry path independent of HRTF positioning.
    mHRTF.process(mReverbWetMono.data(), mHrtfL.data(), mHrtfR.data(), nFrames, azimuth, elevation, distance);
  }
  else
  {
    // Fallback keeps wet path from the reverb stage, bypasses distance boost,
    // and hard-limits to avoid accidental clipping when HRTF is unavailable.
    spatialtail::FillStereoFromFallbackWetMono(mReverbWetMono.data(), mHrtfL.data(), mHrtfR.data(), nFrames);
  }

  // Inverse-distance gain: 1 m is the reference (gain = 1.0).
  // When HRTF is unavailable, fallback gain is forced to unity.
  const float targetGain = spatialtail::ComputeWetDistanceGain(distance, hrtfLoaded);

  // Mix dry mono + wet binaural (with smoothed distance gain) into stereo outputs
  for (int s = 0; s < nFrames; ++s)
  {
    mSmoothedDistanceGain += mDistanceSmoothCoeff * (targetGain - mSmoothedDistanceGain);
    const float wet = mSmoothedDistanceGain * dryWet;
    const float dry = mDryAlignedMono[s] * (1.f - dryWet);
    outputs[0][s] = static_cast<sample>(dry + mHrtfL[s] * wet);
    outputs[1][s] = static_cast<sample>(dry + mHrtfR[s] * wet);
  }
}
#endif
