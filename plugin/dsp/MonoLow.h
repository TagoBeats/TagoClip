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

// Linkwitz-Riley 4th order mono-maker: everything below freq is summed to mid,
// the high band stays stereo. Port of tagodsp.stereo.mono_low.MonoLow
// (LR4 = two cascaded Butterworth biquads per band, sums allpass-flat).
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

    void process (float* left, float* right, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const double l = left[i];
            const double r = right[i];
            const double lowL = lp[0][1].process (lp[0][0].process (l));
            const double lowR = lp[1][1].process (lp[1][0].process (r));
            const double highL = hp[0][1].process (hp[0][0].process (l));
            const double highR = hp[1][1].process (hp[1][0].process (r));
            const double mono = 0.5 * (lowL + lowR);
            left[i] = (float) (highL + mono);
            right[i] = (float) (highR + mono);
        }
    }

    void reset() noexcept
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < 2; ++s)
            {
                lp[ch][s].reset();
                hp[ch][s].reset();
            }
    }

private:
    void updateCoefficients() noexcept
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < 2; ++s)
            {
                lp[ch][s].setButterworth (false, freqHz, sr);
                hp[ch][s].setButterworth (true, freqHz, sr);
            }
    }

    double sr = 44100.0;
    double freqHz = 120.0;
    Biquad lp[2][2], hp[2][2];
};
} // namespace tagoclip
