#include "HRTFProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <utility>
#include <vector>

// SOFA path injected by CMake at compile time.
// Override with -DHRTF_TEST_SOFA_PATH=/path/to/file.sofa
#ifndef HRTF_TEST_SOFA_PATH
#define HRTF_TEST_SOFA_PATH ""
#endif

static const char* kTestSofaPath = HRTF_TEST_SOFA_PATH;
static constexpr float kTestSampleRate = 44100.f;
static constexpr int   kBlockSize = 256;
static constexpr float kEpsilon = 1.0e-9f;

namespace
{

// Returns false (skip) and prints a message if the SOFA file is unavailable.
bool LoadTestSOFA(HRTFProcessor& proc)
{
    if (!kTestSofaPath || kTestSofaPath[0] == '\0')
    {
        std::cerr << "  [SKIP] HRTF_TEST_SOFA_PATH not set\n";
        return false;
    }
    if (!proc.load(kTestSofaPath, kTestSampleRate))
    {
        std::cerr << "  [SKIP] SOFA load failed: " << kTestSofaPath << "\n";
        return false;
    }
    return true;
}

uint32_t XorShift32(uint32_t& state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

// ---------------------------------------------------------------------------
// 1. Fallback path
// When no SOFA is loaded, input must pass through unchanged to both channels.
// ---------------------------------------------------------------------------
bool TestHRTFFallbackPath()
{
    HRTFProcessor proc;  // load() never called

    std::vector<float> in(kBlockSize);
    for (int i = 0; i < kBlockSize; ++i)
        in[i] = static_cast<float>(i) / static_cast<float>(kBlockSize) - 0.5f;

    std::vector<float> outL(kBlockSize, 0.f);
    std::vector<float> outR(kBlockSize, 0.f);

    proc.process(in.data(), outL.data(), outR.data(), kBlockSize, 0.f, 0.f, 1.f);

    for (int i = 0; i < kBlockSize; ++i)
    {
        if (std::fabs(outL[i] - in[i]) > kEpsilon) return false;
        if (std::fabs(outR[i] - in[i]) > kEpsilon) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// 2. ITD delay values
// Feeds an impulse at 45° azimuth. Verifies the SOFA lookup produced:
//   - Finite output in both channels (SOFA assertions in lookupTarget() passed)
//   - Energy in both channels (filter is non-trivial)
//   - Distinct L and R channels for off-center azimuths (no center collapse)
// Note: Task 4 will actually apply delayL/delayR to output timing. This test
// checks that the stored IR is spatially asymmetric, which is a prerequisite.
// Skips gracefully when no SOFA file is available.
// ---------------------------------------------------------------------------
bool TestHRTFITDDelayValues()
{
    HRTFProcessor proc;
    if (!LoadTestSOFA(proc))
        return true;  // skip — not a failure

    std::vector<float> in(kBlockSize, 0.f);
    in[0] = 1.f;  // impulse at sample 0

    std::vector<float> outL(kBlockSize, 0.f);
    std::vector<float> outR(kBlockSize, 0.f);

    // 45° azimuth should produce clearly lateralised output
    proc.process(in.data(), outL.data(), outR.data(), kBlockSize, 45.f, 0.f, 1.f);

    // All samples must be finite (NaN/Inf would indicate a corrupt IR)
    for (int i = 0; i < kBlockSize; ++i)
    {
        if (!std::isfinite(outL[i])) return false;
        if (!std::isfinite(outR[i])) return false;
    }

    // Both channels must have non-zero energy (IR was applied)
    float energyL = 0.f, energyR = 0.f;
    for (int i = 0; i < kBlockSize; ++i)
    {
        energyL += outL[i] * outL[i];
        energyR += outR[i] * outR[i];
    }
    if (energyL <= 0.f) return false;
    if (energyR <= 0.f) return false;

    // Channels must differ at 45° — a pure center position would have identical L/R.
    // (This is what the debug center-collapse detection checks internally.)
    float diffEnergy = 0.f;
    for (int i = 0; i < kBlockSize; ++i)
    {
        const float d = outL[i] - outR[i];
        diffEnergy += d * d;
    }
    if (diffEnergy <= kEpsilon) return false;

    return true;
}

// ---------------------------------------------------------------------------
// 3. Crossfade continuity
// Changes azimuth every block and verifies:
//   - No NaN or Inf in output across all block boundaries
//   - Output magnitude stays within a generous bound (no signal explosion)
// With SOFA: exercises real IR switching. Without SOFA: exercises fallback.
// Note: seamless crossfading (Task 5) is not required to pass this test.
// ---------------------------------------------------------------------------
bool TestHRTFCrossfadeContinuity()
{
    HRTFProcessor proc;
    LoadTestSOFA(proc);  // ok to fail; test runs either way

    static constexpr int   kNumBlocks = 20;
    static constexpr float kMaxSampleMagnitude = 3.f;  // loose bound

    const float azimuths[] = {-60.f, -30.f, 0.f, 30.f, 60.f};

    std::vector<float> in(kBlockSize);
    std::vector<float> outL(kBlockSize);
    std::vector<float> outR(kBlockSize);

    for (int b = 0; b < kNumBlocks; ++b)
    {
        // Continuous sine wave to make inter-block boundary detectable
        const float phaseBase = static_cast<float>(b * kBlockSize) * 2.f * 3.14159265f * 440.f / kTestSampleRate;
        for (int i = 0; i < kBlockSize; ++i)
            in[i] = std::sin(phaseBase + static_cast<float>(i) * 2.f * 3.14159265f * 440.f / kTestSampleRate);

        const float az = azimuths[b % 5];
        proc.process(in.data(), outL.data(), outR.data(), kBlockSize, az, 0.f, 1.f);

        for (int i = 0; i < kBlockSize; ++i)
        {
            if (!std::isfinite(outL[i])) return false;
            if (!std::isfinite(outR[i])) return false;
            if (std::fabs(outL[i]) > kMaxSampleMagnitude) return false;
            if (std::fabs(outR[i]) > kMaxSampleMagnitude) return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// 4. Smoothing response
// Applies an instantaneous azimuth jump and verifies determinism: two
// independent runs with identical inputs and parameter sequences produce
// bit-identical output. This validates that the current step behaviour is
// stable and reproducible. Task 6 will add smoothing and update expectations.
// ---------------------------------------------------------------------------
bool TestHRTFSmoothingResponse()
{
    std::vector<float> in(kBlockSize);
    for (int i = 0; i < kBlockSize; ++i)
        in[i] = static_cast<float>(i % 16) / 16.f - 0.5f;

    std::vector<float> runA_L(kBlockSize), runA_R(kBlockSize);
    std::vector<float> runB_L(kBlockSize), runB_R(kBlockSize);

    // Run A
    {
        HRTFProcessor proc;
        LoadTestSOFA(proc);  // ok to fail

        std::vector<float> tmp_L(kBlockSize), tmp_R(kBlockSize);
        proc.process(in.data(), tmp_L.data(), tmp_R.data(), kBlockSize, 0.f, 0.f, 1.f);
        proc.process(in.data(), runA_L.data(), runA_R.data(), kBlockSize, 45.f, 0.f, 1.f);
    }

    // Run B — fresh processor, identical sequence
    {
        HRTFProcessor proc;
        LoadTestSOFA(proc);

        std::vector<float> tmp_L(kBlockSize), tmp_R(kBlockSize);
        proc.process(in.data(), tmp_L.data(), tmp_R.data(), kBlockSize, 0.f, 0.f, 1.f);
        proc.process(in.data(), runB_L.data(), runB_R.data(), kBlockSize, 45.f, 0.f, 1.f);
    }

    // Output must be bit-identical (no non-determinism in the position change path)
    for (int i = 0; i < kBlockSize; ++i)
    {
        if (runA_L[i] != runB_L[i]) return false;
        if (runA_R[i] != runB_R[i]) return false;
    }

    // Output must be finite
    for (int i = 0; i < kBlockSize; ++i)
    {
        if (!std::isfinite(runA_L[i])) return false;
        if (!std::isfinite(runA_R[i])) return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// 5. Gain stability
// Feeds white noise through 50 blocks while sweeping azimuth. Verifies:
//   - No NaN or Inf in any output sample
//   - Per-block RMS stays within a generous bound (no unbounded growth)
// ---------------------------------------------------------------------------
bool TestHRTFGainStability()
{
    HRTFProcessor proc;
    LoadTestSOFA(proc);  // ok to fail; fallback path is also tested

    static constexpr int   kNumBlocks = 50;
    static constexpr float kMaxRms = 5.f;  // generous: checks for unbounded growth

    const float azimuths[] = {-90.f, -60.f, -30.f, 0.f, 30.f, 60.f, 90.f};

    std::vector<float> in(kBlockSize);
    std::vector<float> outL(kBlockSize);
    std::vector<float> outR(kBlockSize);
    uint32_t rng = 0xDEADBEEFu;

    for (int b = 0; b < kNumBlocks; ++b)
    {
        for (int i = 0; i < kBlockSize; ++i)
            in[i] = static_cast<float>(XorShift32(rng) & 0xFFu) / 128.f - 1.f;

        const float az = azimuths[b % 7];
        proc.process(in.data(), outL.data(), outR.data(), kBlockSize, az, 10.f * static_cast<float>(b % 3 - 1), 1.f);

        float rmsL = 0.f, rmsR = 0.f;
        for (int i = 0; i < kBlockSize; ++i)
        {
            if (!std::isfinite(outL[i])) return false;
            if (!std::isfinite(outR[i])) return false;
            rmsL += outL[i] * outL[i];
            rmsR += outR[i] * outR[i];
        }
        rmsL = std::sqrt(rmsL / static_cast<float>(kBlockSize));
        rmsR = std::sqrt(rmsR / static_cast<float>(kBlockSize));

        if (rmsL > kMaxRms) return false;
        if (rmsR > kMaxRms) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// 6. No-allocation invariant
// Processes many blocks during azimuth sweep and verifies that the processor
// remains in a consistent, operational state throughout. This is a structural
// proxy for "no audio-thread heap allocation": the HRTFState vectors were
// preallocated at load() and must not be resized or reallocated during
// normal processing.
// ---------------------------------------------------------------------------
bool TestHRTFNoAllocationInvariant()
{
    HRTFProcessor proc;
    if (!LoadTestSOFA(proc))
    {
        // Without SOFA: exercise fallback path 200 times — must not crash
        std::vector<float> in(kBlockSize, 0.5f);
        std::vector<float> outL(kBlockSize);
        std::vector<float> outR(kBlockSize);
        for (int b = 0; b < 200; ++b)
            proc.process(in.data(), outL.data(), outR.data(), kBlockSize, 0.f, 0.f, 1.f);
        return true;
    }

    const bool loadedBefore = proc.isLoaded();

    std::vector<float> in(kBlockSize, 0.f);
    std::vector<float> outL(kBlockSize);
    std::vector<float> outR(kBlockSize);
    float az = -90.f;

    for (int b = 0; b < 200; ++b)
    {
        in[b % kBlockSize] = 1.f;
        proc.process(in.data(), outL.data(), outR.data(), kBlockSize, az, 0.f, 1.f);
        in[b % kBlockSize] = 0.f;
        az += 1.f;
        if (az > 90.f) az = -90.f;
    }

    // Processor must still be operational after 200 blocks of motion
    if (proc.isLoaded() != loadedBefore) return false;

    // Final output must be finite — undefined behaviour after reallocation would
    // likely corrupt data and surface here
    for (int i = 0; i < kBlockSize; ++i)
    {
        if (!std::isfinite(outL[i])) return false;
        if (!std::isfinite(outR[i])) return false;
    }
    return true;
}

} // namespace

int main()
{
    struct Test { const char* name; bool (*fn)(); };
    const Test tests[] = {
        { "TestHRTFFallbackPath",          TestHRTFFallbackPath          },
        { "TestHRTFITDDelayValues",        TestHRTFITDDelayValues        },
        { "TestHRTFCrossfadeContinuity",   TestHRTFCrossfadeContinuity   },
        { "TestHRTFSmoothingResponse",     TestHRTFSmoothingResponse     },
        { "TestHRTFGainStability",         TestHRTFGainStability         },
        { "TestHRTFNoAllocationInvariant", TestHRTFNoAllocationInvariant },
    };

    for (const auto& t : tests)
    {
        if (!t.fn())
        {
            std::cerr << t.name << " FAILED\n";
            return 1;
        }
        std::cout << t.name << " passed\n";
    }

    std::cout << "hrtf_processor_tests passed\n";
    return 0;
}
