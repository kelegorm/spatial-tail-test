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

  // Crossfade length: fixed at load time.
  mCrossfadeLengthSamples = kDefaultCrossfadeSamples;
  mCrossfadeRemaining = 0;

  // Position sentinels: force the first HRTF lookup in process().
  mLastAzimuthDeg  = 1e10f;
  mLastElevationDeg = 1e10f;
  mLastDistanceM   = 1e10f;

  // Crossfade "from" delays: reset at load time.
  mFromDelayL = 0.f;
  mFromDelayR = 0.f;

  // ITD delay buffers: preallocated here, never resized on the audio thread.
  // kMaxITDDelaySamples is sized conservatively (see header for rationale).
  mSampleRate = sampleRate;
  mDelayBufL.assign(kMaxITDDelaySamples, 0.f);
  mDelayBufR.assign(kMaxITDDelaySamples, 0.f);
  mDelayPosL = 0;
  mDelayPosR = 0;

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
  (void)mDebug.disableSmoothing;
#endif

  // Only re-look up the HRTF when position changes beyond a small threshold.
  // This prevents restarting an in-progress crossfade on every block when the
  // host keeps sending the same parameter values, and avoids redundant SOFA work.
  static constexpr float kPosTolerance = 0.1f;  // degrees / metres
  const bool posChanged =
      (std::fabs(azimuthDeg  - mLastAzimuthDeg)   > kPosTolerance ||
       std::fabs(elevationDeg - mLastElevationDeg) > kPosTolerance ||
       std::fabs(distanceM   - mLastDistanceM)     > kPosTolerance);

  if (posChanged)
  {
    lookupTarget(azimuthDeg, elevationDeg, distanceM);
    mLastAzimuthDeg   = azimuthDeg;
    mLastElevationDeg = elevationDeg;
    mLastDistanceM    = distanceM;
  }

  // Handle newly looked-up target: start or update the crossfade.
  if (mTargetPending)
  {
    bool doCrossfade = true;
#ifndef NDEBUG
    doCrossfade = !mDebug.disableCrossfade;
    if (mDebug.logTransition)
    {
      fprintf(stderr,
              "[HRTF] transition: %s  delayL=%.6f delayR=%.6f  crossfadeRemaining=%d\n",
              (mCrossfadeRemaining == 0) ? "start crossfade" : "update target mid-crossfade",
              mTargetState.delayL, mTargetState.delayR, mCrossfadeRemaining);
    }
#endif

    if (!doCrossfade)
    {
      // Bypass: instant switch with no blending.
      mCurrentState = mTargetState;  // no heap alloc; same-size vectors
      mCrossfadeRemaining = 0;
    }
    else if (mCrossfadeRemaining == 0)
    {
      // No crossfade in progress: start one from the current committed state.
      // Save "from" ITD delays for per-sample interpolation in the ITD stage.
      mFromDelayL = mCurrentState.delayL;
      mFromDelayR = mCurrentState.delayR;
      mCrossfadeRemaining = mCrossfadeLengthSamples;
    }
    // else: crossfade is already running; mTargetState has been updated by
    // lookupTarget(). The ongoing crossfade continues from mCurrentState toward
    // the new target at its current alpha position — no reset, no discontinuity
    // in the "from" side (mCurrentState is unchanged).

    mTargetPending = false;
  }

  // Save crossfade state before the FIR phase modifies mCrossfadeRemaining,
  // so the ITD stage can compute matching per-sample alpha values.
  const int crossfadeRemainingAtStart = mCrossfadeRemaining;
  const int nCrossfade = std::min(nFrames, crossfadeRemainingAtStart);

  // -------------------------------------------------------------------------
  // FIR convolution — two phases:
  //   Phase 1 (crossfade): blend mCurrentState and mTargetState outputs per
  //                        sample. Requires running both FIRs, but is bounded
  //                        to mCrossfadeLengthSamples samples total.
  //   Phase 2 (normal):    single FIR with committed mCurrentState only.
  //
  // Both phases share the single circular input buffer; no heap allocation.
  // Processing order: FIR first (spectral shaping), then ITD delay below.
  // -------------------------------------------------------------------------
  const int N = mFilterLength;

  // Phase 1: crossfade portion.
  for (int i = 0; i < nCrossfade; ++i)
  {
    mInputBuffer[mBufPos] = in[i];

    float fromL = 0.f, fromR = 0.f, toL = 0.f, toR = 0.f;
    for (int k = 0; k < N; ++k)
    {
      const int   idx = (mBufPos - k + N) % N;
      const float s   = mInputBuffer[idx];
      fromL += mCurrentState.irL[k] * s;
      fromR += mCurrentState.irR[k] * s;
      toL   += mTargetState.irL[k]  * s;
      toR   += mTargetState.irR[k]  * s;
    }

    // alpha = 0 at the very start of the crossfade, 1 at the very end.
    // (crossfadeRemainingAtStart - i) is the count of samples still to process
    // including this one, so alpha rises as remaining falls.
    const float alpha = 1.f - static_cast<float>(crossfadeRemainingAtStart - i)
                              / static_cast<float>(mCrossfadeLengthSamples);
    outL[i] = (1.f - alpha) * fromL + alpha * toL;
    outR[i] = (1.f - alpha) * fromR + alpha * toR;

    mBufPos = (mBufPos + 1) % N;
  }

  // Decrement crossfade counter; commit target when the crossfade completes.
  // The copy does not heap-allocate: mCurrentState and mTargetState were
  // preallocated to mFilterLength at load() and have the same capacity.
  mCrossfadeRemaining -= nCrossfade;
  if (mCrossfadeRemaining == 0 && crossfadeRemainingAtStart > 0)
    mCurrentState = mTargetState;

  // Phase 2: normal FIR (mCurrentState may have just been committed above).
  for (int i = nCrossfade; i < nFrames; ++i)
  {
    mInputBuffer[mBufPos] = in[i];

    float sumL = 0.f, sumR = 0.f;
    for (int k = 0; k < N; ++k)
    {
      const int idx = (mBufPos - k + N) % N;
      sumL += mCurrentState.irL[k] * mInputBuffer[idx];
      sumR += mCurrentState.irR[k] * mInputBuffer[idx];
    }

    outL[i] = sumL;
    outR[i] = sumR;

    mBufPos = (mBufPos + 1) % N;
  }

  // -------------------------------------------------------------------------
  // ITD (interaural time difference) delay stage.
  //
  // During crossfade samples, the delay is linearly interpolated per sample
  // between the "from" delay (mFromDelayL/R, captured at crossfade start) and
  // the target delay (mTargetState.delayL/R). This mirrors the FIR blend and
  // avoids a sudden ITD jump at the filter switch point.
  //
  // After the crossfade, mCurrentState == mTargetState, so both paths produce
  // the same committed delay. Delay values are in seconds; convert to fractional
  // samples using mSampleRate. Linear interpolation provides sub-sample accuracy.
  //
  // Clamping: [0, kMaxITDDelaySamples - 2] keeps both the integer tap and the
  // (integer+1) interpolation tap within the preallocated delay buffer.
  // -------------------------------------------------------------------------
#ifndef NDEBUG
  const bool applyITD = !mDebug.disableITD;
#else
  constexpr bool applyITD = true;
#endif

  if (applyITD)
  {
    const float maxDelaySamples = static_cast<float>(kMaxITDDelaySamples - 2);

    for (int i = 0; i < nFrames; ++i)
    {
      // Compute effective delay: interpolate during crossfade portion, use
      // committed mCurrentState delay for the normal portion.
      float effDelL, effDelR;
      if (i < nCrossfade && crossfadeRemainingAtStart > 0)
      {
        const float alpha = 1.f - static_cast<float>(crossfadeRemainingAtStart - i)
                                  / static_cast<float>(mCrossfadeLengthSamples);
        effDelL = (1.f - alpha) * mFromDelayL + alpha * mTargetState.delayL;
        effDelR = (1.f - alpha) * mFromDelayR + alpha * mTargetState.delayR;
      }
      else
      {
        effDelL = mCurrentState.delayL;
        effDelR = mCurrentState.delayR;
      }

      // Convert seconds → fractional samples; clamp to buffer bounds.
      const float rawL     = effDelL * mSampleRate;
      const float rawR     = effDelR * mSampleRate;
      const float clampedL = rawL < 0.f ? 0.f : (rawL > maxDelaySamples ? maxDelaySamples : rawL);
      const float clampedR = rawR < 0.f ? 0.f : (rawR > maxDelaySamples ? maxDelaySamples : rawR);

      const int   intL  = static_cast<int>(clampedL);
      const float fracL = clampedL - static_cast<float>(intL);
      const int   intR  = static_cast<int>(clampedR);
      const float fracR = clampedR - static_cast<float>(intR);

      // Write FIR output into per-ear delay line, then read back at the delay tap.
      // Linear interpolation: output = (1-frac)*buf[pos-int] + frac*buf[pos-int-1].
      mDelayBufL[mDelayPosL] = outL[i];
      {
        const int r0 = (mDelayPosL - intL     + kMaxITDDelaySamples) % kMaxITDDelaySamples;
        const int r1 = (mDelayPosL - intL - 1 + kMaxITDDelaySamples) % kMaxITDDelaySamples;
        outL[i] = (1.f - fracL) * mDelayBufL[r0] + fracL * mDelayBufL[r1];
      }
      mDelayPosL = (mDelayPosL + 1) % kMaxITDDelaySamples;

      mDelayBufR[mDelayPosR] = outR[i];
      {
        const int r0 = (mDelayPosR - intR     + kMaxITDDelaySamples) % kMaxITDDelaySamples;
        const int r1 = (mDelayPosR - intR - 1 + kMaxITDDelaySamples) % kMaxITDDelaySamples;
        outR[i] = (1.f - fracR) * mDelayBufR[r0] + fracR * mDelayBufR[r1];
      }
      mDelayPosR = (mDelayPosR + 1) % kMaxITDDelaySamples;
    }
  }
}
