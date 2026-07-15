#pragma once

#include <array>
#include <memory>

#include <juce_audio_basics/juce_audio_basics.h>

#include "dsp/Curves.h"
#include "dsp/MonoLow.h"
#include "dsp/Oversampler.h"

namespace tagoclip
{
struct Parameters
{
    curves::Type curve = curves::Type::fl;
    int thresholdSteps = 100; // t = steps / 128, Fruity scale
    float driveDb = 0.0f;
    int oversample = 1;       // 1, 4 or 8; default off (Robin, 15.07.2026)
    float outputDb = 0.0f;
    float monoLow = 0.0f;     // normalized log sweep, < 0.03 = off, freq = 20 * 20^v Hz
    bool delta = false;
    static constexpr float monoLowOffBelow = 0.03f;
};

// Fixed delay of Oversampler::totalLatency samples, keeps the os-off wet path
// and the delta dry path on the same grid as the resampled path.
class LatencyDelay
{
public:
    void reset() noexcept
    {
        buf.fill (0.0f);
        pos = 0;
    }

    float process (float x) noexcept
    {
        const float out = buf[(size_t) pos];
        buf[(size_t) pos] = x;
        pos = pos + 1 == (int) buf.size() ? 0 : pos + 1;
        return out;
    }

private:
    std::array<float, Oversampler::totalLatency> buf {};
    int pos = 0;
};

// Signal chain, mirrored 1:1 by golden/make_reference.py:
//   mono-low (LR4, pre clipper) -> drive -> curve with oversampling
//   -> optional delta (driven dry minus wet, i.e. exactly what the curve
//      removed, in positive polarity) -> output gain
// Latency is a constant 20 samples in every mode: the resampler pair costs
// 10 + 10, the os-off path is delayed to match so toggling never moves PDC
// (hosts compensate, the Fruity 1:1 null test still works).
class ClipEngine
{
public:
    void prepare (double sampleRate, int numChannels, const Parameters& initial)
    {
        channels = juce::jlimit (1, 2, numChannels);
        params = pending = initial;

        monoLow.prepare (sampleRate);
        if (params.monoLow >= Parameters::monoLowOffBelow)
            monoLow.setFrequency (monoLowFreq (params.monoLow));

        for (auto& c : chans)
        {
            if (c.os4 == nullptr)
            {
                c.os4 = std::make_unique<Oversampler> (4);
                c.os8 = std::make_unique<Oversampler> (8);
            }
            c.os4->reset();
            c.os8->reset();
            c.comp.reset();
            c.deltaDelay.reset();
        }

        driveGain.reset (sampleRate, 0.02);
        outGain.reset (sampleRate, 0.02);
        driveGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.driveDb));
        outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.outputDb));
    }

    void setParameters (const Parameters& p) noexcept { pending = p; }

    int latencySamples() const noexcept { return Oversampler::totalLatency; }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        applyPending();

        const int n = buffer.getNumSamples();
        const int numCh = juce::jmin (channels, buffer.getNumChannels());
        float* left = buffer.getWritePointer (0);
        float* right = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

        if (right != nullptr && params.monoLow >= Parameters::monoLowOffBelow)
            monoLow.process (left, right, n);

        const double t = params.thresholdSteps / 128.0;
        for (int i = 0; i < n; ++i)
        {
            const float gd = driveGain.getNextValue();
            const float go = outGain.getNextValue();
            for (int c = 0; c < numCh; ++c)
            {
                auto& st = chans[(size_t) c];
                float* data = c == 0 ? left : right;
                const float driven = data[i] * gd;

                float wet;
                if (params.oversample == 1)
                {
                    wet = st.comp.process (curves::apply (params.curve, driven, t));
                }
                else
                {
                    auto& os = params.oversample == 4 ? *st.os4 : *st.os8;
                    float hi[8];
                    os.upsample (driven, hi);
                    for (int p = 0; p < params.oversample; ++p)
                        hi[p] = curves::apply (params.curve, hi[p], t);
                    wet = os.downsample (hi);
                }

                const float dryDelayed = st.deltaDelay.process (driven);
                data[i] = (params.delta ? dryDelayed - wet : wet) * go;
            }
        }
    }

private:
    static double monoLowFreq (float v) noexcept
    {
        return 20.0 * std::pow (20.0, (double) v);
    }

    void applyPending() noexcept
    {
        if (pending.oversample != params.oversample)
            for (auto& c : chans)
            {
                c.os4->reset();
                c.os8->reset();
                c.comp.reset();
                c.deltaDelay.reset();
            }

        const bool monoOn = pending.monoLow >= Parameters::monoLowOffBelow;
        if (monoOn)
        {
            if (params.monoLow < Parameters::monoLowOffBelow)
                monoLow.reset();
            monoLow.setFrequency (monoLowFreq (pending.monoLow));
        }

        driveGain.setTargetValue (juce::Decibels::decibelsToGain (pending.driveDb));
        outGain.setTargetValue (juce::Decibels::decibelsToGain (pending.outputDb));
        params = pending;
    }

    struct Channel
    {
        std::unique_ptr<Oversampler> os4, os8;
        LatencyDelay comp;       // aligns the os-off wet path
        LatencyDelay deltaDelay; // aligns the driven dry path for delta
    };

    Parameters params, pending;
    int channels = 2;
    MonoLow monoLow;
    std::array<Channel, 2> chans;
    juce::SmoothedValue<float> driveGain { 1.0f }, outGain { 1.0f };
};
} // namespace tagoclip
