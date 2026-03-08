#include "ReverbStage.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
constexpr double kEpsilon = 1.0e-9;

bool TestMonoCopyContract()
{
  const std::vector<float> mono = {0.0f, 0.25f, -0.75f, 1.0f, -1.0f};
  std::vector<double> reverbL(mono.size(), 0.0);
  std::vector<double> reverbR(mono.size(), 0.0);

  spatialtail::CopyMonoToStereoReverbInputs(mono.data(), reverbL.data(), reverbR.data(), static_cast<int>(mono.size()));

  for (size_t i = 0; i < mono.size(); ++i)
  {
    if (std::fabs(reverbL[i] - mono[i]) > kEpsilon) return false;
    if (std::fabs(reverbR[i] - mono[i]) > kEpsilon) return false;
  }

  return true;
}

bool TestReverbDefaultsInExpectedRange()
{
  return spatialtail::kReverbDefaultRoomSize > 0.0 && spatialtail::kReverbDefaultRoomSize < 1.0
      && spatialtail::kReverbDefaultDamping >= 0.0 && spatialtail::kReverbDefaultDamping <= 1.0
      && spatialtail::kReverbDefaultWidth >= -1.0 && spatialtail::kReverbDefaultWidth <= 1.0;
}

bool TestResetClearsTail()
{
  constexpr int kFrames = 256;
  constexpr int kWarmupBlocks = 40;
  constexpr int kTailProbeBlocks = 40;
  WDL_ReverbEngine reverb;
  spatialtail::ResetReverb(reverb, 48000.0);

  std::vector<double> inL(kFrames, 1.0);
  std::vector<double> inR(kFrames, 1.0);
  std::vector<double> outL(kFrames, 0.0);
  std::vector<double> outR(kFrames, 0.0);
  std::vector<double> silentL(kFrames, 0.0);
  std::vector<double> silentR(kFrames, 0.0);

  for (int i = 0; i < kWarmupBlocks; ++i)
    reverb.ProcessSampleBlock(inL.data(), inR.data(), outL.data(), outR.data(), kFrames);

  double preResetTailMax = 0.0;
  for (int b = 0; b < kTailProbeBlocks; ++b)
  {
    reverb.ProcessSampleBlock(silentL.data(), silentR.data(), outL.data(), outR.data(), kFrames);
    for (int i = 0; i < kFrames; ++i)
      preResetTailMax = std::max(preResetTailMax, std::max(std::fabs(outL[i]), std::fabs(outR[i])));
  }

  if (preResetTailMax <= 0.0)
    return false;

  spatialtail::ResetReverb(reverb, 48000.0);
  reverb.ProcessSampleBlock(silentL.data(), silentR.data(), outL.data(), outR.data(), kFrames);

  double postResetTailMax = 0.0;
  for (int i = 0; i < kFrames; ++i)
    postResetTailMax = std::max(postResetTailMax, std::max(std::fabs(outL[i]), std::fabs(outR[i])));

  return postResetTailMax <= kEpsilon;
}
} // namespace

int main()
{
  if (!TestMonoCopyContract())
  {
    std::cerr << "TestMonoCopyContract failed\n";
    return 1;
  }

  if (!TestReverbDefaultsInExpectedRange())
  {
    std::cerr << "TestReverbDefaultsInExpectedRange failed\n";
    return 1;
  }

  if (!TestResetClearsTail())
  {
    std::cerr << "TestResetClearsTail failed\n";
    return 1;
  }

  std::cout << "reverb_stage_tests passed\n";
  return 0;
}
