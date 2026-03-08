#pragma once

#include <vector>

struct MYSOFA_EASY;

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
  void updateFilter(float azimuthDeg, float elevationDeg, float distanceM);

  MYSOFA_EASY* mEasy = nullptr;
  int mFilterLength = 0;
  std::vector<float> mIRLeft;
  std::vector<float> mIRRight;
  std::vector<float> mInputBuffer;
  int mBufPos = 0;
};
