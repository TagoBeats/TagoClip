#pragma once

#include <algorithm>
#include <array>

#include "OversamplerTaps.h"

namespace tagoclip
{
// Streaming polyphase up/down pair, numerically matching
// scipy.signal.resample_poly with the kaiser beta 12 window from
// tagodsp.distortion.clipper (taps shared via OversamplerTaps.h).
// Group delay is groupDelayBase base-rate samples per direction,
// independent of the factor.
class Oversampler
{
public:
    // Derived from the generated taps (numTaps = 2 * delay * factor + 1), so a
    // regenerated filter design updates latency, delay buffers and PDC in sync.
    static constexpr int groupDelayBase = (taps::numTaps4x - 1) / (2 * 4);
    static_assert (taps::numTaps4x == 2 * groupDelayBase * 4 + 1, "4x taps out of sync");
    static_assert (taps::numTaps8x == 2 * groupDelayBase * 8 + 1, "8x taps out of sync");
    static constexpr int totalLatency = 2 * groupDelayBase;

    explicit Oversampler (int factorToUse) : factor (factorToUse)
    {
        const double* h = factor == 4 ? taps::taps4x : taps::taps8x;
        numTaps = factor == 4 ? taps::numTaps4x : taps::numTaps8x;
        downTaps = h;

        // Upsampling branch p holds h[p + m*factor] * factor.
        for (int p = 0; p < factor; ++p)
            for (int m = 0; m < branchLen; ++m)
                if (p + m * factor < numTaps)
                    branches[(size_t) (p * branchLen + m)] = h[p + m * factor] * factor;

        reset();
    }

    void reset() noexcept
    {
        upHist.fill (0.0);
        downRing.fill (0.0f);
        upPos = 0;
        downPos = 0;
    }

    // One base-rate sample in, factor high-rate samples out (into hi).
    void upsample (float x, float* hi) noexcept
    {
        // Double-length ring written newest-first, so hist[m] is sample n-m.
        upPos = upPos == 0 ? branchLen - 1 : upPos - 1;
        upHist[(size_t) upPos] = x;
        upHist[(size_t) (upPos + branchLen)] = x;
        const double* hist = upHist.data() + upPos;
        for (int p = 0; p < factor; ++p)
        {
            const double* b = branches.data() + (size_t) (p * branchLen);
            double acc = 0.0;
            for (int m = 0; m < branchLen; ++m)
                acc += b[m] * hist[m];
            hi[p] = (float) acc;
        }
    }

    // factor high-rate samples in, one base-rate sample out. The dot is taken
    // right after phase 0 lands so the output stays on scipy's sample grid.
    float downsample (const float* hi) noexcept
    {
        push (hi[0]);
        double acc = 0.0;
        const float* w = downRing.data() + downPos;
        // The filter is symmetric, so this forward dot over the chronological
        // window equals the convolution with reversed taps.
        for (int j = 0; j < numTaps; ++j)
            acc += downTaps[j] * (double) w[j];
        for (int p = 1; p < factor; ++p)
            push (hi[p]);
        return (float) acc;
    }

private:
    static constexpr int branchLen = 2 * groupDelayBase + 1;
    static constexpr int maxTaps = taps::numTaps8x;
    static constexpr int maxFactor = 8;

    void push (float v) noexcept
    {
        downRing[(size_t) downPos] = v;
        downRing[(size_t) (downPos + numTaps)] = v;
        downPos = downPos + 1 == numTaps ? 0 : downPos + 1;
    }

    int factor = 8;
    int numTaps = 0;
    const double* downTaps = nullptr;
    std::array<double, maxFactor * branchLen> branches {};
    // Double-length rings so both dots always see a contiguous window.
    std::array<double, 2 * branchLen> upHist {};
    std::array<float, 2 * maxTaps> downRing {};
    int upPos = 0;
    int downPos = 0;
};
} // namespace tagoclip
