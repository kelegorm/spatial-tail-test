#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "HRTFProcessor.h"
#include "ReverbStage.h"
#include <vector>

const int kNumPresets = 1;

enum EParams
{
  kAzimuth = 0,
  kElevation,
  kDistance,
  kDryWet,
  kNumParams
};

using namespace iplug;
using namespace igraphics;

class SpatialTail final : public Plugin
{
public:
  SpatialTail(const InstanceInfo& info);

#if IPLUG_DSP // http://bit.ly/2S64BDd
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void OnReset() override;
#endif

private:
  WDL_ReverbEngine mReverb;
  HRTFProcessor mHRTF;
  std::vector<float> mMonoIn;
  std::vector<double> mReverbInL;
  std::vector<double> mReverbInR;
  std::vector<double> mReverbOutL;
  std::vector<double> mReverbOutR;
  std::vector<float> mHrtfL;
  std::vector<float> mHrtfR;
  float mSmoothedDistanceGain = 1.f;
  float mDistanceSmoothCoeff  = 0.f;
  double mLastSampleRate = 0.0;
};
