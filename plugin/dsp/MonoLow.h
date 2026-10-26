#pragma once

#include <cmath>
#include <cstring>

namespace tagoclip
{
// RBJ cookbook biquad, transposed direct form II, double math throughout.
// Port of tagodsp.filters.biquad (q fixed at Butterworth for the LR4 bands).
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setButterworth (bool highpassMode, double f0, double sr) noexcept
    {
        constexpr double q = 0.7071067811865476;
        const double w0 = 2.0 * 3.141592653589793 * f0 / sr;
        const double alpha = std::sin (w0) / (2.0 * q);
        const double cosw = std::cos (w0);
        const double a0 = 1.0 + alpha;
        if (highpassMode)
        {
            b0 = (1.0 + cosw) / 2.0 / a0;
            b1 = -(1.0 + cosw) / a0;
            b2 = (1.0 + cosw) / 2.0 / a0;
        }
        else
        {
            b0 = (1.0 - cosw) / 2.0 / a0;
            b1 = (1.0 - cosw) / a0;
            b2 = (1.0 - cosw) / 2.0 / a0;
        }
        a1 = -2.0 * cosw / a0;
        a2 = (1.0 - alpha) / a0;
    }

    void reset() noexcept { z1 = z2 = 0.0; }
};

// Mono-maker: mid/side split, the mid passes untouched and only the side goes
// through a Linkwitz-Riley 4th order highpass (two cascaded Butterworth
// biquads). Port of tagodsp.stereo.mono_low.MonoLow.
//
// Mid content is bit identical to the input at every setting, also while the
// frequency moves. The earlier L/R crossover ran the mid through the LR4
// allpass, which swung its phase audibly whenever the knob moved and bumped on
// engage (Ry, beta.3, 25.10.2026). Since the mid is never rotated, the dry side
// of the mix needs no matching allpass either.
class MonoLow
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        updateCoefficients();
        reset();
    }

    void setFrequency (double f) noexcept
    {
        // Bitwise compare: only skip the coefficient update for the exact same value.
        if (std::memcmp (&f, &freqHz, sizeof (double)) == 0)
            return;
        freqHz = f;
        updateCoefficients();
    }

    void processSample (float& left, float& right) noexcept
    {
        const double l = left;
        const double r = right;
        const double mid = 0.5 * (l + r);
        const double side = hp[1].process (hp[0].process (0.5 * (l - r)));
        left = (float) (mid + side);
        right = (float) (mid - side);
    }

    void process (float* left, float* right, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            processSample (left[i], right[i]);
    }

    void reset() noexcept
    {
        for (auto& s : hp)
            s.reset();
    }

private:
    void updateCoefficients() noexcept
    {
        for (auto& s : hp)
            s.setButterworth (true, freqHz, sr);
    }

    double sr = 44100.0;
    double freqHz = 120.0;
    Biquad hp[2]; // side highpass, LR4 = two cascaded Butterworth stages
};
} // namespace tagoclip
