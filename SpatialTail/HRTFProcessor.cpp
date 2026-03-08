#include "HRTFProcessor.h"

#include <algorithm>
#include <cstring>

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

  // Look up the new target for this block and store it in mTargetState.
  lookupTarget(azimuthDeg, elevationDeg, distanceM);

  // Apply the pending target: abrupt switch for now (Task 5 will crossfade).
  // mCrossfadeRemaining is managed here once crossfade logic is added in Task 5.
  if (mTargetPending)
  {
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
