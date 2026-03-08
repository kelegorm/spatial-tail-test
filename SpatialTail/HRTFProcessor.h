#pragma once

#include <vector>

struct MYSOFA_EASY;

// Holds one complete HRTF filter state (both ears, plus ITD delays).
// Preallocated at load time; never resized on the audio thread.
struct HRTFState
{
  std::vector<float> irL;
  std::vector<float> irR;
  float delayL = 0.f;  // ITD fractional delay, left ear (seconds)
  float delayR = 0.f;  // ITD fractional delay, right ear (seconds)

  void resize(int filterLength)
  {
    irL.assign(filterLength, 0.f);
    irR.assign(filterLength, 0.f);
    delayL = 0.f;
    delayR = 0.f;
  }
};

class HRTFProcessor
{
public:
  HRTFProcessor();
  ~HRTFProcessor();

  HRTFProcessor(const HRTFProcessor&) = delete;
  HRTFProcessor& operator=(const HRTFProcessor&) = delete;
  HRTFProcessor(HRTFProcessor&&) = delete;
  HRTFProcessor& operator=(HRTFProcessor&&) = delete;

  bool load(const char* sofaPath, float sampleRate);
  void process(const float* in, float* outL, float* outR, int nFrames,
               float azimuthDeg, float elevationDeg, float distanceM);

  bool isLoaded() const { return mEasy != nullptr; }

private:
  // Writes a new HRTF lookup result into mTargetState and sets mTargetPending.
  // Never writes directly to mCurrentState; called once per process() block.
  void lookupTarget(float azimuthDeg, float elevationDeg, float distanceM);

  MYSOFA_EASY* mEasy = nullptr;
  int mFilterLength = 0;

  // Currently active HRTF state — used for convolution on the audio thread.
  HRTFState mCurrentState;

  // Newly looked-up HRTF state — written by lookupTarget(), consumed by process().
  HRTFState mTargetState;

  // True when mTargetState holds a freshly looked-up filter not yet applied.
  bool mTargetPending = false;

  // Circular input buffer for time-domain FIR convolution.
  std::vector<float> mInputBuffer;
  int mBufPos = 0;

  // Crossfade transition state (preallocated; used by Task 5).
  // mCrossfadeLengthSamples is set once at load() time.
  // mCrossfadeRemaining counts down to 0 as the crossfade progresses.
  int mCrossfadeLengthSamples = 0;
  int mCrossfadeRemaining = 0;
};
