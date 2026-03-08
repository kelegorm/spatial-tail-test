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
} // namespace spatialtail
