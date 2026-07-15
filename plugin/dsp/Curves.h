#pragma once

#include <algorithm>
#include <cmath>

// Transfer curves ported 1:1 from tagodsp.distortion.clipper. All are
// odd-symmetric, identity below threshold t, ceiling 1.0 (hard: ceiling = t,
// same as the Python prototype). Math in double to track the float64 reference.
namespace tagoclip::curves
{
enum class Type
{
    fl = 0,
    hard = 1,
    tanh = 2
};

// Fruity Soft Clipper, reverse-measured 2026-07-15 (TagoClip/measure):
// y = sign(x) * (1 - (1-t) * exp(-(|x|-t)/(1-t))) above t, identity below.
inline float flSoftclip (float x, double t) noexcept
{
    const double ax = std::abs ((double) x);
    if (ax <= t)
        return x;
    const double knee = 1.0 - (1.0 - t) * std::exp (-(ax - t) / (1.0 - t));
    return (float) (x < 0.0f ? -knee : knee);
}

inline float hardClip (float x, double t) noexcept
{
    return (float) std::clamp ((double) x, -t, t);
}

inline float tanhClip (float x, double t) noexcept
{
    const double ax = std::abs ((double) x);
    if (ax <= t)
        return x;
    const double knee = t + (1.0 - t) * std::tanh ((ax - t) / (1.0 - t));
    return (float) (x < 0.0f ? -knee : knee);
}

inline float apply (Type type, float x, double t) noexcept
{
    switch (type)
    {
        case Type::hard: return hardClip (x, t);
        case Type::tanh: return tanhClip (x, t);
        case Type::fl:   break;
    }
    return flSoftclip (x, t);
}
} // namespace tagoclip::curves
