#include "HRTFProcessor.h"

#include <cstring>

#include "hrtf/mysofa.h"

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

  mIRLeft.assign(mFilterLength, 0.f);
  mIRRight.assign(mFilterLength, 0.f);
  mInputBuffer.assign(mFilterLength, 0.f);
  mBufPos = 0;

  return true;
}

void HRTFProcessor::updateFilter(float azimuthDeg, float elevationDeg, float distanceM)
{
  if (!mEasy)
    return;

  // mysofa_s2c converts [azimuth_deg, elevation_deg, radius] to Cartesian [x,y,z].
  // SOFA/AES convention: positive azimuth is to the LEFT (counterclockwise from above).
  // GUI convention: positive X (azimuth param) is to the RIGHT.
  // Negate azimuth here so that moving the XY pad right puts the source on the right.
  float coords[3] = { -azimuthDeg, elevationDeg, distanceM };
  mysofa_s2c(coords);

  // delayL/delayR (ITD fractional delays) intentionally ignored for MVP
  float delayL, delayR;
  mysofa_getfilter_float(mEasy, coords[0], coords[1], coords[2],
                         mIRLeft.data(), mIRRight.data(), &delayL, &delayR);
  (void)delayL;
  (void)delayR;
}

void HRTFProcessor::process(const float* in, float* outL, float* outR, int nFrames,
                             float azimuthDeg, float elevationDeg, float distanceM)
{
  if (!mEasy)
  {
    memcpy(outL, in, nFrames * sizeof(float));
    memcpy(outR, in, nFrames * sizeof(float));
    return;
  }

  updateFilter(azimuthDeg, elevationDeg, distanceM);

  const int N = mFilterLength;
  for (int i = 0; i < nFrames; ++i)
  {
    mInputBuffer[mBufPos] = in[i];

    float sumL = 0.f, sumR = 0.f;
    for (int k = 0; k < N; ++k)
    {
      int idx = (mBufPos - k + N) % N;
      sumL += mIRLeft[k] * mInputBuffer[idx];
      sumR += mIRRight[k] * mInputBuffer[idx];
    }

    outL[i] = sumL;
    outR[i] = sumR;

    mBufPos = (mBufPos + 1) % N;
  }
}
