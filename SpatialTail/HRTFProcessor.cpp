#include "HRTFProcessor.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>

#ifndef NDEBUG
#  include <cstdio>
#endif

#include "hrtf/mysofa.h"

// Default crossfade length in samples (applies at typical 44.1/48 kHz rates).
// Task 5 will use this; set conservatively so it is ready without touching load().
static constexpr int kDefaultCrossfadeSamples = 128;

HRTFProcessor::HRTFProcessor() {}

HRTFProcessor::~HRTFProcessor()
{
  if (mEasy)
  {
    mysofa_close(mEasy);
    mEasy = nullptr;
  }
}

bool HRTFProcessor::load(const char* sofaPath, float sampleRate)
{
  if (mEasy)
  {
    mysofa_close(mEasy);
    mEasy = nullptr;
  }

  int err = 0;
  mEasy = mysofa_open(sofaPath, sampleRate, &mFilterLength, &err);
  if (!mEasy || err != 0 || mFilterLength <= 0)
  {
    if (mEasy)
      mysofa_close(mEasy);
    mEasy = nullptr;
    return false;
  }

  // Preallocate both current and target IR storage; no further heap allocation
  // occurs on the audio thread.
  mCurrentState.resize(mFilterLength);
  mTargetState.resize(mFilterLength);

  // Circular input buffer for FIR convolution.
  mInputBuffer.assign(mFilterLength, 0.f);
  mBufPos = 0;

  // Crossfade length: fixed at load time, used by the transition logic in Task 5.
  mCrossfadeLengthSamples = kDefaultCrossfadeSamples;
  mCrossfadeRemaining = 0;

  mTargetPending = false;

  return true;
}

#ifndef NDEBUG
// Returns sum of |irL[k] - irR[k]| for the given state.
// Used to detect center collapse (both ears receiving an identical response).
static float computeIRDifference(const HRTFState& state, int filterLength)
{
  float sum = 0.f;
  for (int k = 0; k < filterLength; ++k)
    sum += std::abs(state.irL[k] - state.irR[k]);
  return sum;
}
#endif

void HRTFProcessor::lookupTarget(float azimuthDeg, float elevationDeg, float distanceM)
{
  if (!mEasy)
    return;

  // Clamp to front-hemisphere bounds regardless of host automation.
  // Azimuth [-90, 90] keeps source in front; elevation [-90, 90] is full range.
  const float az = std::max(-90.f, std::min(90.f, azimuthDeg));
  const float el = std::max(-90.f, std::min(90.f, elevationDeg));
  const float dist = std::max(0.1f, distanceM);

  // mysofa_s2c converts [azimuth_deg, elevation_deg, radius] to Cartesian [x,y,z].
  // SOFA/AES convention: positive azimuth is to the LEFT (counterclockwise from above).
  // GUI convention: positive X (azimuth param) is to the RIGHT.
  // Negate azimuth here so that moving the XY pad right puts the source on the right.
  float coords[3] = { -az, el, dist };
  mysofa_s2c(coords);

  // Write lookup result into mTargetState (not mCurrentState).
  // delayL/delayR are ITD fractional delays in seconds — stored for Task 4.
  mysofa_getfilter_float(mEasy, coords[0], coords[1], coords[2],
                         mTargetState.irL.data(), mTargetState.irR.data(),
                         &mTargetState.delayL, &mTargetState.delayR);

#ifndef NDEBUG
  // Validate ITD delay values: must be finite and physically plausible.
  // Maximum ITD at ~0.09 m head radius ≈ 0.00066 s; add margin for SOFA data variation.
  assert(!std::isnan(mTargetState.delayL) && "delayL is NaN after SOFA lookup");
  assert(!std::isnan(mTargetState.delayR) && "delayR is NaN after SOFA lookup");
  assert(mTargetState.delayL >= 0.f    && "delayL is negative");
  assert(mTargetState.delayR >= 0.f    && "delayR is negative");
  // Warn (not assert) for unusually large values — some SOFA files may differ.
  if (mTargetState.delayL > 0.01f || mTargetState.delayR > 0.01f)
  {
    fprintf(stderr, "[HRTF] WARNING: unusually large ITD delay: delayL=%.6f delayR=%.6f\n",
            mTargetState.delayL, mTargetState.delayR);
  }

  if (mDebug.logLookup)
  {
    const float irDiff = computeIRDifference(mTargetState, mFilterLength);
    fprintf(stderr,
            "[HRTF] lookup az=%.2f el=%.2f dist=%.3f  delayL=%.6f delayR=%.6f  irDiff=%.6f%s\n",
            az, el, dist,
            mTargetState.delayL, mTargetState.delayR,
            irDiff,
            irDiff < 1e-6f ? "  *** CENTER COLLAPSE DETECTED ***" : "");
  }
#endif

  mTargetPending = true;
}

void HRTFProcessor::process(const float* in, float* outL, float* outR, int nFrames,
                             float azimuthDeg, float elevationDeg, float distanceM)
{
  if (!mEasy)
  {
    // Fallback: pass input through to both channels unchanged.
    memcpy(outL, in, nFrames * sizeof(float));
    memcpy(outR, in, nFrames * sizeof(float));
    return;
  }

#ifndef NDEBUG
  // Acknowledge bypass flags consumed by future tasks so the compiler does not
  // warn about unused fields in builds where those tasks are not yet active.
  (void)mDebug.disableITD;
  (void)mDebug.disableSmoothing;
#endif

  // Look up the new target for this block and store it in mTargetState.
  lookupTarget(azimuthDeg, elevationDeg, distanceM);

  // Apply the pending target: abrupt switch for now (Task 5 will crossfade).
  // mCrossfadeRemaining is managed here once crossfade logic is added in Task 5.
  if (mTargetPending)
  {
#ifndef NDEBUG
    if (mDebug.logTransition)
    {
      fprintf(stderr,
              "[HRTF] transition: committing new target  delayL=%.6f delayR=%.6f  crossfadeRemaining=%d\n",
              mTargetState.delayL, mTargetState.delayR, mCrossfadeRemaining);
    }
    // disableCrossfade flag: respected here so Task 5 can be bypassed for testing.
    // (No-op until Task 5 implements crossfade logic; included now for future use.)
    (void)mDebug.disableCrossfade;
#endif
    mCurrentState = mTargetState;  // copy preallocated vectors — no heap alloc
    mTargetPending = false;
    mCrossfadeRemaining = 0;       // no crossfade yet; Task 5 will set this
  }

  // Time-domain FIR convolution using mCurrentState IRs and the circular input buffer.
  const int N = mFilterLength;
  for (int i = 0; i < nFrames; ++i)
  {
    mInputBuffer[mBufPos] = in[i];

    float sumL = 0.f, sumR = 0.f;
    for (int k = 0; k < N; ++k)
    {
      int idx = (mBufPos - k + N) % N;
      sumL += mCurrentState.irL[k] * mInputBuffer[idx];
      sumR += mCurrentState.irR[k] * mInputBuffer[idx];
    }

    outL[i] = sumL;
    outR[i] = sumR;

    mBufPos = (mBufPos + 1) % N;
  }
}
