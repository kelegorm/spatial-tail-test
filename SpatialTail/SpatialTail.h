#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "HRTFProcessor.h"
#include "ReverbStage.h"
#include <limits>
#include <vector>

const int kNumPresets = 1;

enum EParams
{
  kAzimuth = 0,
  kElevation,
  kDistance,
  kDryWet,
  kReverbRoomSize,
  kReverbDamping,
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
  void OnActivate(bool active) override;
#endif

private:
  WDL_ReverbEngine mReverb;
  HRTFProcessor mHRTF;
  std::vector<float> mMonoIn;
  std::vector<double> mReverbInL;
  std::vector<double> mReverbInR;
  std::vector<double> mReverbOutL;
  std::vector<double> mReverbOutR;
  std::vector<float> mDryAlignedMono;
  std::vector<float> mDryDelayLine;
  std::vector<float> mReverbWetMono;
  std::vector<float> mHrtfL;
  std::vector<float> mHrtfR;
  int mReverbLatencySamples = 0;
  int mReverbTailSamples = 0;
  int mDryDelayWritePos = 0;
  float mSmoothedDistanceGain = 1.f;
  float mSmoothedReverbRoomSize = static_cast<float>(spatialtail::kReverbDefaultRoomSize);
  float mSmoothedReverbDamping = static_cast<float>(spatialtail::kReverbDefaultDamping);
  float mAppliedReverbRoomSize = std::numeric_limits<float>::quiet_NaN();
  float mAppliedReverbDamping = std::numeric_limits<float>::quiet_NaN();
  float mDistanceSmoothCoeff  = 0.f;
  double mLastSampleRate = 0.0;
  bool mReverbInitialized = false;
  bool mForceFullDspReset = false;
};
