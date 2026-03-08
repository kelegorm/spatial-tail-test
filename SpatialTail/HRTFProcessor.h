#pragma once

#include <vector>

struct MYSOFA_EASY;

// ---------------------------------------------------------------------------
// Debug instrumentation — present only in Debug builds (NDEBUG not defined).
// ---------------------------------------------------------------------------
#ifndef NDEBUG

// Feature-bypass and logging flags for isolated testing and diagnosis.
// Set individual fields before processing to enable instrumentation.
// logLookup and logTransition print to stderr — enable only during investigation.
struct HRTFDebugFlags
{
  // Logging
  bool logLookup     = false;  // log az/el/dist + ITD per process() block
  bool logTransition = false;  // log when a pending target is committed

  // Feature bypass (no-ops until the corresponding task implements the feature)
  bool disableITD       = false;  // bypass ITD delay application  (Task 4)
  bool disableCrossfade = false;  // bypass crossfade transitions   (Task 5)
  bool disableSmoothing = false;  // bypass az/el smoothing          (Task 6)
};

#endif // !NDEBUG

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

  // ITD fractional delay buffers (Task 4).
  // Processing order: FIR convolution first (spectral HRTF shaping), then per-ear
  // delay (interaural time difference). Delay values from SOFA are in seconds;
  // they are converted to samples at process() time using mSampleRate.
  //
  // kMaxITDDelaySamples must exceed the largest ITD the SOFA data can produce.
  // Physical maximum for a human head (~0.09 m radius): ~660 µs ≈ 127 samples at
  // 192 kHz. 256 samples provides a safe margin for any reasonable SOFA file.
  static constexpr int kMaxITDDelaySamples = 256;

  float mSampleRate = 44100.f;         // stored at load() for seconds→samples conversion
  std::vector<float> mDelayBufL;       // per-ear circular delay line (left)
  std::vector<float> mDelayBufR;       // per-ear circular delay line (right)
  int mDelayPosL = 0;                  // current write position in mDelayBufL
  int mDelayPosR = 0;                  // current write position in mDelayBufR

#ifndef NDEBUG
public:
  // Accessible from tests and host code; not present in Release builds.
  HRTFDebugFlags mDebug;

#endif
};
