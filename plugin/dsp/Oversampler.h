#pragma once

#include <algorithm>
#include <vector>

#include "OversamplerTaps.h"

namespace tagoclip
{
// Streaming polyphase up/down pair, numerically matching
// scipy.signal.resample_poly with the kaiser beta 12 window from
// tagodsp.distortion.clipper (taps shared via OversamplerTaps.h).
// Group delay is 10 base-rate samples per direction, 20 in total,
// independent of the factor.
class Oversampler
{
public:
    static constexpr int totalLatency = 20;

    explicit Oversampler (int factorToUse) : factor (factorToUse)
    {
        const double* h = factor == 4 ? taps::taps4x : taps::taps8x;
        numTaps = factor == 4 ? taps::numTaps4x : taps::numTaps8x;
        downTaps = h;

        // Upsampling branch p holds h[p + m*factor] * factor.
        branchLen = (numTaps + factor - 1) / factor;
        branches.resize ((size_t) (factor * branchLen), 0.0);
        for (int p = 0; p < factor; ++p)
            for (int m = 0; m < branchLen; ++m)
                if (p + m * factor < numTaps)
                    branches[(size_t) (p * branchLen + m)] = h[p + m * factor] * factor;

        upHist.resize ((size_t) branchLen);
        // Double-length ring so the downsampling dot always sees a contiguous window.
        downRing.resize ((size_t) (2 * numTaps));
        reset();
    }

    void reset() noexcept
    {
        std::fill (upHist.begin(), upHist.end(), 0.0f);
        std::fill (downRing.begin(), downRing.end(), 0.0f);
        downPos = 0;
    }

    // One base-rate sample in, factor high-rate samples out (into hi).
    void upsample (float x, float* hi) noexcept
    {
        std::move_backward (upHist.begin(), upHist.end() - 1, upHist.end());
        upHist[0] = x;
        for (int p = 0; p < factor; ++p)
        {
            const double* b = branches.data() + (size_t) (p * branchLen);
            double acc = 0.0;
            for (int m = 0; m < branchLen; ++m)
                acc += b[m] * (double) upHist[(size_t) m];
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
    void push (float v) noexcept
    {
        downRing[(size_t) downPos] = v;
        downRing[(size_t) (downPos + numTaps)] = v;
        downPos = downPos + 1 == numTaps ? 0 : downPos + 1;
    }

    int factor = 8;
    int numTaps = 0;
    int branchLen = 0;
    const double* downTaps = nullptr;
    std::vector<double> branches;
    std::vector<float> upHist;
    std::vector<float> downRing;
    int downPos = 0;
};
} // namespace tagoclip
