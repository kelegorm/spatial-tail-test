#include "ReverbStage.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
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

bool TestInputFoldDownContract()
{
  std::vector<float> ch0 = {1.0f, -1.0f, 0.5f, 0.0f};
  std::vector<float> ch1 = {0.0f, 1.0f, -0.5f, 1.0f};
  std::vector<float> mono(ch0.size(), 0.f);

  float* stereoInputs[2] = {
    ch0.data(),
    ch1.data()
  };

  spatialtail::FoldDownToMono(stereoInputs, 2, mono.data(), static_cast<int>(mono.size()));

  for (size_t i = 0; i < mono.size(); ++i)
  {
    const float expected = 0.5f * (ch0[i] + ch1[i]);
    if (std::fabs(mono[i] - expected) > static_cast<float>(kEpsilon)) return false;
  }

  std::fill(mono.begin(), mono.end(), 0.f);
  float* monoInput[1] = {ch0.data()};
  spatialtail::FoldDownToMono(monoInput, 1, mono.data(), static_cast<int>(mono.size()));

  for (size_t i = 0; i < mono.size(); ++i)
  {
    if (std::fabs(mono[i] - ch0[i]) > static_cast<float>(kEpsilon)) return false;
  }

  return true;
}

bool TestReverbOutputCollapseContract()
{
  const std::vector<double> reverbL = {1.0, -0.5, 0.25, -1.0};
  const std::vector<double> reverbR = {-1.0, -0.5, 0.75, 1.0};
  std::vector<float> wetMono(reverbL.size(), 0.f);

  spatialtail::CollapseStereoReverbToMono(reverbL.data(), reverbR.data(), wetMono.data(), static_cast<int>(wetMono.size()));

  for (size_t i = 0; i < wetMono.size(); ++i)
  {
    const double expected = 0.5 * (reverbL[i] + reverbR[i]);
    if (std::fabs(static_cast<double>(wetMono[i]) - expected) > kEpsilon) return false;
  }

  return true;
}

bool TestReverbDefaultsInExpectedRange()
{
  return spatialtail::kReverbDefaultRoomSize > 0.0 && spatialtail::kReverbDefaultRoomSize < 1.0
      && spatialtail::kReverbDefaultDamping >= 0.0 && spatialtail::kReverbDefaultDamping <= 1.0
      && spatialtail::kReverbDefaultWidth >= -1.0 && spatialtail::kReverbDefaultWidth <= 1.0;
}

bool TestReverbParameterRangeContract()
{
  if (spatialtail::kReverbRoomMin < 0.0 || spatialtail::kReverbRoomMax > 1.0 || spatialtail::kReverbRoomMin >= spatialtail::kReverbRoomMax)
    return false;

  if (spatialtail::kReverbDampingMin < 0.0 || spatialtail::kReverbDampingMax > 1.0 || spatialtail::kReverbDampingMin >= spatialtail::kReverbDampingMax)
    return false;

  if (std::fabs(spatialtail::ClampReverbRoomSize(-1.0f) - static_cast<float>(spatialtail::kReverbRoomMin)) > static_cast<float>(kEpsilon))
    return false;

  if (std::fabs(spatialtail::ClampReverbRoomSize(2.0f) - static_cast<float>(spatialtail::kReverbRoomMax)) > static_cast<float>(kEpsilon))
    return false;

  if (std::fabs(spatialtail::ClampReverbDamping(-1.0f) - static_cast<float>(spatialtail::kReverbDampingMin)) > static_cast<float>(kEpsilon))
    return false;

  if (std::fabs(spatialtail::ClampReverbDamping(2.0f) - static_cast<float>(spatialtail::kReverbDampingMax)) > static_cast<float>(kEpsilon))
    return false;

  return true;
}

bool TestReverbAutomationSmoothing()
{
  const float coeff = spatialtail::ComputeBlockSmoothingCoefficient(48000.0, 256, spatialtail::kAutomationSmoothingTimeSeconds);
  if (coeff <= 0.f || coeff >= 1.f)
    return false;

  const float start = 0.15f;
  const float target = 0.95f;
  float smoothed = start;

  const float first = spatialtail::SmoothTowards(smoothed, target, coeff);
  if (first <= start || first >= target)
    return false;

  smoothed = first;
  for (int i = 0; i < 48; ++i)
  {
    const float next = spatialtail::SmoothTowards(smoothed, target, coeff);
    if (next < smoothed || next > target + static_cast<float>(kEpsilon))
      return false;
    smoothed = next;
  }

  // After roughly a quarter-second of blocks, the smoothed value should be
  // near target but still have changed gradually.
  if (std::fabs(smoothed - target) > 1.0e-3f)
    return false;

  const float fullJump = target - start;
  const float smoothedFirstJump = first - start;
  if (smoothedFirstJump >= fullJump)
    return false;

  return true;
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

bool TestHostTimingMeasurement()
{
  const auto timing44k = spatialtail::MeasureReverbHostTiming(44100.0);
  if (timing44k.latencySamples <= 0) return false;
  if (timing44k.tailSamples <= timing44k.latencySamples) return false;

  const auto timing48k = spatialtail::MeasureReverbHostTiming(48000.0);
  if (timing48k.latencySamples <= 0) return false;
  if (timing48k.tailSamples <= timing48k.latencySamples) return false;

  return true;
}

bool TestDryAlignmentDelay()
{
  std::vector<float> delayLine;
  int writePos = 0;
  constexpr int delaySamples = 2;
  spatialtail::PrepareMonoDelayLine(delayLine, delaySamples, writePos);

  const std::vector<float> inA = {1.f, 2.f, 3.f, 4.f};
  std::vector<float> outA(inA.size(), 0.f);
  spatialtail::ApplyMonoDelay(inA.data(), outA.data(), static_cast<int>(inA.size()), delaySamples, delayLine, writePos);

  const std::vector<float> expectedA = {0.f, 0.f, 1.f, 2.f};
  for (size_t i = 0; i < outA.size(); ++i)
  {
    if (std::fabs(outA[i] - expectedA[i]) > static_cast<float>(kEpsilon)) return false;
  }

  const std::vector<float> inB = {5.f, 6.f};
  std::vector<float> outB(inB.size(), 0.f);
  spatialtail::ApplyMonoDelay(inB.data(), outB.data(), static_cast<int>(inB.size()), delaySamples, delayLine, writePos);

  const std::vector<float> expectedB = {3.f, 4.f};
  for (size_t i = 0; i < outB.size(); ++i)
  {
    if (std::fabs(outB[i] - expectedB[i]) > static_cast<float>(kEpsilon)) return false;
  }

  std::vector<float> passThrough(3, 0.f);
  const std::vector<float> inNoDelay = {0.5f, -0.25f, 1.f};
  spatialtail::ApplyMonoDelay(inNoDelay.data(), passThrough.data(), static_cast<int>(inNoDelay.size()), 0, delayLine, writePos);

  for (size_t i = 0; i < inNoDelay.size(); ++i)
  {
    if (std::fabs(passThrough[i] - inNoDelay[i]) > static_cast<float>(kEpsilon)) return false;
  }

  return true;
}

bool TestDebugHRTFLoadFailureToggle()
{
  if (unsetenv(spatialtail::kForceHRTFLoadFailureEnvVar) != 0)
    return false;

  if (spatialtail::ShouldForceHRTFLoadFailureInDebug())
    return false;

  const char* defaultPath = "/tmp/default.sofa";
  if (std::string(spatialtail::ResolveHRTFLoadPath(defaultPath)) != defaultPath)
    return false;

  if (setenv(spatialtail::kForceHRTFLoadFailureEnvVar, "1", 1) != 0)
    return false;

#if !defined(NDEBUG)
  if (!spatialtail::ShouldForceHRTFLoadFailureInDebug())
    return false;
  if (std::string(spatialtail::ResolveHRTFLoadPath(defaultPath)) != spatialtail::kForcedInvalidSofaPath)
    return false;
#else
  if (spatialtail::ShouldForceHRTFLoadFailureInDebug())
    return false;
  if (std::string(spatialtail::ResolveHRTFLoadPath(defaultPath)) != defaultPath)
    return false;
#endif

  if (unsetenv(spatialtail::kForceHRTFLoadFailureEnvVar) != 0)
    return false;

  return true;
}

bool TestFallbackGainAndClippingGuard()
{
  if (std::fabs(spatialtail::ComputeWetDistanceGain(0.1f, false) - 1.f) > static_cast<float>(kEpsilon))
    return false;
  if (std::fabs(spatialtail::ComputeWetDistanceGain(10.f, false) - 1.f) > static_cast<float>(kEpsilon))
    return false;
  if (std::fabs(spatialtail::ComputeWetDistanceGain(0.1f, true) - 10.f) > static_cast<float>(kEpsilon))
    return false;

  const std::vector<float> wetMono = {1.5f, -1.25f, 0.6f, -0.3f};
  std::vector<float> outL(wetMono.size(), 0.f);
  std::vector<float> outR(wetMono.size(), 0.f);
  spatialtail::FillStereoFromFallbackWetMono(wetMono.data(), outL.data(), outR.data(), static_cast<int>(wetMono.size()));

  const std::vector<float> expected = {1.f, -1.f, 0.6f, -0.3f};
  for (size_t i = 0; i < expected.size(); ++i)
  {
    if (std::fabs(outL[i] - expected[i]) > static_cast<float>(kEpsilon))
      return false;
    if (std::fabs(outR[i] - expected[i]) > static_cast<float>(kEpsilon))
      return false;
  }

  return true;
}
} // namespace

int main()
{
  if (!TestInputFoldDownContract())
  {
    std::cerr << "TestInputFoldDownContract failed\n";
    return 1;
  }

  if (!TestMonoCopyContract())
  {
    std::cerr << "TestMonoCopyContract failed\n";
    return 1;
  }

  if (!TestReverbOutputCollapseContract())
  {
    std::cerr << "TestReverbOutputCollapseContract failed\n";
    return 1;
  }

  if (!TestReverbDefaultsInExpectedRange())
  {
    std::cerr << "TestReverbDefaultsInExpectedRange failed\n";
    return 1;
  }

  if (!TestReverbParameterRangeContract())
  {
    std::cerr << "TestReverbParameterRangeContract failed\n";
    return 1;
  }

  if (!TestReverbAutomationSmoothing())
  {
    std::cerr << "TestReverbAutomationSmoothing failed\n";
    return 1;
  }

  if (!TestResetClearsTail())
  {
    std::cerr << "TestResetClearsTail failed\n";
    return 1;
  }

  if (!TestHostTimingMeasurement())
  {
    std::cerr << "TestHostTimingMeasurement failed\n";
    return 1;
  }

  if (!TestDryAlignmentDelay())
  {
    std::cerr << "TestDryAlignmentDelay failed\n";
    return 1;
  }

  if (!TestDebugHRTFLoadFailureToggle())
  {
    std::cerr << "TestDebugHRTFLoadFailureToggle failed\n";
    return 1;
  }

  if (!TestFallbackGainAndClippingGuard())
  {
    std::cerr << "TestFallbackGainAndClippingGuard failed\n";
    return 1;
  }

  std::cout << "reverb_stage_tests passed\n";
  return 0;
}
