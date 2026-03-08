#pragma once

#include "../libs/iPlug2/WDL/verbengine.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace spatialtail
{
// Baseline reverb voicing for predictable wet-path behavior in later routing tasks.
constexpr double kReverbDefaultRoomSize = 0.72;
constexpr double kReverbDefaultDamping = 0.30;
constexpr double kReverbDefaultWidth = 1.0;
constexpr double kReverbRoomMin = 0.0;
constexpr double kReverbRoomMax = 0.99;
constexpr double kReverbDampingMin = 0.0;
constexpr double kReverbDampingMax = 1.0;
constexpr double kAutomationSmoothingTimeSeconds = 0.02;
constexpr float kReverbTuningApplyEpsilon = 1.0e-6f;
constexpr double kReverbLatencyDetectThreshold = 1.0e-12;
constexpr double kReverbTailDetectThreshold = 1.0e-6;
constexpr double kReverbTailMaxProbeSeconds = 20.0;
constexpr double kReverbTailSilenceWindowSeconds = 0.25;
constexpr double kRealtimeSafeLatencySeconds = 1214.0 / 48000.0;
constexpr const char* kForceHRTFLoadFailureEnvVar = "SPATIALTAIL_DEBUG_FORCE_HRTF_LOAD_FAILURE";
constexpr const char* kForcedInvalidSofaPath = "/__spatialtail__/forced-invalid.sofa";

struct ReverbHostTiming
{
  int latencySamples = 0;
  int tailSamples = 0;
  bool tailTruncated = false;
};

inline void ConfigureReverbDefaults(WDL_ReverbEngine& reverb)
{
  reverb.SetRoomSize(kReverbDefaultRoomSize);
  reverb.SetDampening(kReverbDefaultDamping);
  reverb.SetWidth(kReverbDefaultWidth);
}

inline float ClampReverbRoomSize(float roomSize)
{
  return std::max(static_cast<float>(kReverbRoomMin), std::min(static_cast<float>(kReverbRoomMax), roomSize));
}

inline float ClampReverbDamping(float damping)
{
  return std::max(static_cast<float>(kReverbDampingMin), std::min(static_cast<float>(kReverbDampingMax), damping));
}

inline void ApplyReverbTuning(WDL_ReverbEngine& reverb, float roomSize, float damping)
{
  reverb.SetRoomSize(ClampReverbRoomSize(roomSize));
  reverb.SetDampening(ClampReverbDamping(damping));
}

inline bool ShouldUpdateReverbTuning(float appliedRoomSize, float appliedDamping,
                                     float candidateRoomSize, float candidateDamping,
                                     float epsilon = kReverbTuningApplyEpsilon)
{
  if (!std::isfinite(appliedRoomSize) || !std::isfinite(appliedDamping))
    return true;

  return std::fabs(candidateRoomSize - appliedRoomSize) > epsilon
      || std::fabs(candidateDamping - appliedDamping) > epsilon;
}

inline bool ApplyReverbTuningRuntime(WDL_ReverbEngine& reverb, float roomSize, float damping,
                                     float& appliedRoomSize, float& appliedDamping)
{
  const float clampedRoom = ClampReverbRoomSize(roomSize);
  const float clampedDamping = ClampReverbDamping(damping);

  if (!ShouldUpdateReverbTuning(appliedRoomSize, appliedDamping, clampedRoom, clampedDamping))
    return false;

  // WDL_ReverbEngine latches room/damping into comb filters on Reset().
  // Reset(false) updates coefficients without clearing the wet tail state.
  reverb.SetRoomSize(clampedRoom);
  reverb.SetDampening(clampedDamping);
  reverb.Reset(false);
  appliedRoomSize = clampedRoom;
  appliedDamping = clampedDamping;
  return true;
}

inline void ResetReverb(WDL_ReverbEngine& reverb, double sampleRate)
{
  reverb.SetSampleRate(sampleRate);
  ConfigureReverbDefaults(reverb);
  reverb.Reset(true);
}

inline float ComputeBlockSmoothingCoefficient(double sampleRate, int blockSize, double smoothingTimeSeconds)
{
  if (sampleRate <= 0.0 || blockSize <= 0 || smoothingTimeSeconds <= 0.0)
    return 1.f;

  const double blockSamples = static_cast<double>(blockSize);
  const double tauSamples = sampleRate * smoothingTimeSeconds;
  if (tauSamples <= 0.0)
    return 1.f;

  return static_cast<float>(1.0 - std::exp(-blockSamples / tauSamples));
}

inline float SmoothTowards(float current, float target, float coefficient)
{
  const float coeff = std::max(0.f, std::min(1.f, coefficient));
  return current + coeff * (target - current);
}

inline bool IsTruthyEnvValue(const char* value)
{
  if (!value)
    return false;

  std::string token(value);
  const auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
  token.erase(token.begin(), std::find_if(token.begin(), token.end(),
                                          [&](unsigned char ch) { return !isSpace(ch); }));
  token.erase(std::find_if(token.rbegin(), token.rend(),
                           [&](unsigned char ch) { return !isSpace(ch); }).base(),
              token.end());
  if (token.empty())
    return false;

  std::transform(token.begin(), token.end(), token.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return token == "1" || token == "true" || token == "yes" || token == "on";
}

inline bool ShouldForceHRTFLoadFailureInDebug()
{
#if !defined(NDEBUG)
  return IsTruthyEnvValue(std::getenv(kForceHRTFLoadFailureEnvVar));
#else
  return false;
#endif
}

inline const char* ResolveHRTFLoadPath(const char* defaultPath)
{
  if (ShouldForceHRTFLoadFailureInDebug())
    return kForcedInvalidSofaPath;

  return defaultPath;
}

inline void CopyMonoToStereoReverbInputs(const float* monoFoldDown, double* reverbInL, double* reverbInR, int nFrames)
{
  for (int s = 0; s < nFrames; ++s)
  {
    const double mono = static_cast<double>(monoFoldDown[s]);
    reverbInL[s] = mono;
    reverbInR[s] = mono;
  }
}

template <typename SampleT>
inline void FoldDownToMono(SampleT* const* inputs, int nInChans, float* monoFoldDown, int nFrames)
{
  if (nInChans <= 0)
  {
    for (int s = 0; s < nFrames; ++s)
      monoFoldDown[s] = 0.f;

    return;
  }

  if (nInChans == 1)
  {
    for (int s = 0; s < nFrames; ++s)
      monoFoldDown[s] = static_cast<float>(inputs[0][s]);

    return;
  }

  for (int s = 0; s < nFrames; ++s)
  {
    double mono = 0.0;
    for (int c = 0; c < nInChans; ++c)
      mono += static_cast<double>(inputs[c][s]);

    monoFoldDown[s] = static_cast<float>(mono / static_cast<double>(nInChans));
  }
}

inline void CollapseStereoReverbToMono(const double* reverbOutL, const double* reverbOutR, float* wetMono, int nFrames)
{
  for (int s = 0; s < nFrames; ++s)
    wetMono[s] = static_cast<float>(0.5 * (reverbOutL[s] + reverbOutR[s]));
}

inline float ComputeWetDistanceGain(float distance, bool hrtfLoaded)
{
  if (!hrtfLoaded)
    return 1.f;

  const float safeDistance = distance < 0.1f ? 0.1f : distance;
  return 1.f / safeDistance;
}

inline float ClampAudioSample(float sample)
{
  return std::max(-1.f, std::min(1.f, sample));
}

inline void FillStereoFromFallbackWetMono(const float* wetMono, float* outL, float* outR, int nFrames)
{
  for (int s = 0; s < nFrames; ++s)
  {
    const float clamped = ClampAudioSample(wetMono[s]);
    outL[s] = clamped;
    outR[s] = clamped;
  }
}

template <typename SampleT, typename OutputT>
inline void FillStereoFromInputFoldDownFallback(SampleT* const* inputs, int nInChans, OutputT* outL, OutputT* outR, int nFrames)
{
  for (int s = 0; s < nFrames; ++s)
  {
    double mono = 0.0;
    int validChannels = 0;
    if (inputs && nInChans > 0)
    {
      for (int c = 0; c < nInChans; ++c)
      {
        if (!inputs[c])
          continue;

        mono += static_cast<double>(inputs[c][s]);
        ++validChannels;
      }
    }

    if (validChannels > 0)
      mono /= static_cast<double>(validChannels);

    const float clamped = ClampAudioSample(static_cast<float>(mono));
    outL[s] = static_cast<OutputT>(clamped);
    outR[s] = static_cast<OutputT>(clamped);
  }
}

inline void PrepareMonoDelayLine(std::vector<float>& delayLine, int delaySamples, int& writePos)
{
  if (delaySamples <= 0)
  {
    delayLine.clear();
    writePos = 0;
    return;
  }

  delayLine.assign(static_cast<size_t>(delaySamples + 1), 0.f);
  writePos = 0;
}

inline void ApplyMonoDelay(const float* input, float* output, int nFrames, int delaySamples, std::vector<float>& delayLine, int& writePos)
{
  if (delaySamples <= 0 || delayLine.empty())
  {
    for (int s = 0; s < nFrames; ++s)
      output[s] = input[s];

    return;
  }

  const int delayLineSize = static_cast<int>(delayLine.size());

  for (int s = 0; s < nFrames; ++s)
  {
    delayLine[writePos] = input[s];
    int readPos = writePos - delaySamples;
    if (readPos < 0)
      readPos += delayLineSize;

    output[s] = delayLine[readPos];
    if (++writePos >= delayLineSize)
      writePos = 0;
  }
}

inline ReverbHostTiming MeasureReverbHostTiming(double sampleRate,
                                                float roomSize = static_cast<float>(kReverbRoomMax),
                                                float damping = static_cast<float>(kReverbDampingMin))
{
  ReverbHostTiming timing;
  if (sampleRate <= 0.0)
    return timing;

  WDL_ReverbEngine probeReverb;
  ResetReverb(probeReverb, sampleRate);
  probeReverb.SetRoomSize(ClampReverbRoomSize(roomSize));
  probeReverb.SetDampening(ClampReverbDamping(damping));
  probeReverb.Reset(false);

  constexpr int kBlockSize = 256;
  const int maxProbeSamples = std::max(kBlockSize, static_cast<int>(sampleRate * kReverbTailMaxProbeSeconds));
  const int silenceWindowSamples = std::max(1, static_cast<int>(sampleRate * kReverbTailSilenceWindowSeconds));

  std::vector<double> inL(kBlockSize, 0.0);
  std::vector<double> inR(kBlockSize, 0.0);
  std::vector<double> outL(kBlockSize, 0.0);
  std::vector<double> outR(kBlockSize, 0.0);

  inL[0] = 1.0;
  inR[0] = 1.0;

  int firstActiveSample = -1;
  int lastActiveSample = -1;
  int processedSamples = 0;

  while (processedSamples < maxProbeSamples)
  {
    probeReverb.ProcessSampleBlock(inL.data(), inR.data(), outL.data(), outR.data(), kBlockSize);

    for (int s = 0; s < kBlockSize && processedSamples < maxProbeSamples; ++s, ++processedSamples)
    {
      const double absSample = std::max(std::fabs(outL[s]), std::fabs(outR[s]));

      if (absSample > kReverbLatencyDetectThreshold && firstActiveSample < 0)
        firstActiveSample = processedSamples;

      if (absSample > kReverbTailDetectThreshold)
        lastActiveSample = processedSamples;
    }

    std::fill(inL.begin(), inL.end(), 0.0);
    std::fill(inR.begin(), inR.end(), 0.0);

    if (firstActiveSample >= 0 && lastActiveSample >= 0 && (processedSamples - lastActiveSample) >= silenceWindowSamples)
      break;
  }

  if (processedSamples >= maxProbeSamples && lastActiveSample >= 0
      && (processedSamples - lastActiveSample) < silenceWindowSamples)
  {
    timing.tailTruncated = true;
  }

  timing.latencySamples = firstActiveSample > 0 ? firstActiveSample : 0;
  timing.tailSamples = lastActiveSample >= 0 ? (lastActiveSample + 1) : 0;
  if (timing.tailSamples < timing.latencySamples)
    timing.tailSamples = timing.latencySamples;

  return timing;
}

inline ReverbHostTiming EstimateRealtimeSafeHostTiming(double sampleRate)
{
  ReverbHostTiming timing;
  if (sampleRate <= 0.0)
    return timing;

  timing.latencySamples = std::max(0, static_cast<int>(std::ceil(sampleRate * kRealtimeSafeLatencySeconds)));
  timing.tailSamples = std::max(
      timing.latencySamples,
      static_cast<int>(std::ceil(sampleRate * kReverbTailMaxProbeSeconds)));
  return timing;
}
} // namespace spatialtail
