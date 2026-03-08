#include "SpatialTail.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"
#include <vector>

// Path to the SOFA file used for development (absolute path, prototype only)
#ifndef DEFAULT_SOFA_PATH
#define DEFAULT_SOFA_PATH "/Users/dmitry/Work/my_own/spatial-tail-test/libs/libmysofa/share/MIT_KEMAR_normal_pinna.sofa"
#endif

SpatialTail::SpatialTail(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kAzimuth)->InitDouble("Azimuth", 0., -180., 180., 1., "deg");
  GetParam(kElevation)->InitDouble("Elevation", 0., -90., 90., 1., "deg");
  GetParam(kDistance)->InitDouble("Distance", 1., 0.1, 10., 0.01, "m");
  GetParam(kDryWet)->InitDouble("Dry/Wet", 1., 0., 1., 0.01, "");

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
    const IRECT versionBounds = innerBounds.GetFromTRHC(300, 20);
    pGraphics->AttachControl(new ITextControl(innerBounds.GetMidVPadded(50), "SpatialTail", IText(50)));
    WDL_String buildInfoStr;
    GetBuildInfoStr(buildInfoStr, __DATE__, __TIME__);
    pGraphics->AttachControl(new ITextControl(versionBounds, buildInfoStr.Get(), DEFAULT_TEXT.WithAlign(EAlign::Far)));
  };
#endif
}

#if IPLUG_DSP
void SpatialTail::OnReset()
{
  mHRTF.load(DEFAULT_SOFA_PATH, static_cast<float>(GetSampleRate()));
}

void SpatialTail::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const float azimuth  = static_cast<float>(GetParam(kAzimuth)->Value());
  const float elevation = static_cast<float>(GetParam(kElevation)->Value());
  const float distance = static_cast<float>(GetParam(kDistance)->Value());
  const float dryWet   = static_cast<float>(GetParam(kDryWet)->Value());

  // Sum stereo input to mono (only 1 input channel configured, but guard anyway)
  const int nInChans = NInChansConnected();
  static thread_local std::vector<float> monoIn;
  monoIn.resize(nFrames, 0.f);

  if (nInChans >= 1)
  {
    for (int s = 0; s < nFrames; ++s)
      monoIn[s] = static_cast<float>(inputs[0][s]);
  }

  // HRTF process into temporary stereo buffers
  static thread_local std::vector<float> hrtfL, hrtfR;
  hrtfL.resize(nFrames);
  hrtfR.resize(nFrames);

  mHRTF.process(monoIn.data(), hrtfL.data(), hrtfR.data(), nFrames, azimuth, elevation, distance);

  // Mix dry mono + wet binaural into stereo outputs
  for (int s = 0; s < nFrames; ++s)
  {
    const float dry = monoIn[s] * (1.f - dryWet);
    outputs[0][s] = static_cast<sample>(dry + hrtfL[s] * dryWet);
    outputs[1][s] = static_cast<sample>(dry + hrtfR[s] * dryWet);
  }
}
#endif
