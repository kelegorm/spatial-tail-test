#pragma once

#include "../libs/iPlug2/WDL/verbengine.h"

namespace spatialtail
{
// Baseline reverb voicing for predictable wet-path behavior in later routing tasks.
constexpr double kReverbDefaultRoomSize = 0.72;
constexpr double kReverbDefaultDamping = 0.30;
constexpr double kReverbDefaultWidth = 1.0;

inline void ConfigureReverbDefaults(WDL_ReverbEngine& reverb)
{
  reverb.SetRoomSize(kReverbDefaultRoomSize);
  reverb.SetDampening(kReverbDefaultDamping);
  reverb.SetWidth(kReverbDefaultWidth);
}

inline void ResetReverb(WDL_ReverbEngine& reverb, double sampleRate)
{
  reverb.SetSampleRate(sampleRate);
  ConfigureReverbDefaults(reverb);
  reverb.Reset(true);
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
} // namespace spatialtail
