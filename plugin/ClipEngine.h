#pragma once

#include <array>
#include <cmath>

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

// Oversampling factor set, shared by the parameter choice list ("Off", "4x",
// "8x"), the engine dispatch and the high-rate scratch buffer size.
inline constexpr int osFactorTable[] = { 1, 4, 8 };
inline constexpr int maxOversample = 8;

// Mono-low sweep mapping, one source for the engine and the UI Hz label.
inline double monoLowFreqHz (float v) noexcept
{
    return 20.0 * std::pow (20.0, (double) v);
}

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
    void prepare (double sampleRate, const Parameters& initial)
    {
        params = pending = initial;

        monoLow.prepare (sampleRate);
        if (params.monoLow >= Parameters::monoLowOffBelow)
            monoLow.setFrequency (monoLowFreqHz (params.monoLow));

        for (auto& c : chans)
        {
            c.os4.reset();
            c.os8.reset();
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
        const int numCh = juce::jmin ((int) chans.size(), buffer.getNumChannels());
        float* chan[2] = { buffer.getWritePointer (0),
                           numCh > 1 ? buffer.getWritePointer (1) : nullptr };

        if (chan[1] != nullptr && params.monoLow >= Parameters::monoLowOffBelow)
            monoLow.process (chan[0], chan[1], n);

        const double t = params.thresholdSteps / 128.0;
        const bool useOs = params.oversample != 1;
        Oversampler* os[2] = {};
        for (int c = 0; c < numCh; ++c)
            os[c] = params.oversample == 4 ? &chans[(size_t) c].os4 : &chans[(size_t) c].os8;

        for (int i = 0; i < n; ++i)
        {
            const float gd = driveGain.getNextValue();
            const float go = outGain.getNextValue();
            for (int c = 0; c < numCh; ++c)
            {
                auto& st = chans[(size_t) c];
                float* data = chan[c];
                const float driven = data[i] * gd;

                float wet;
                if (! useOs)
                {
                    wet = st.comp.process (curves::apply (params.curve, driven, t));
                }
                else
                {
                    float hi[maxOversample];
                    os[c]->upsample (driven, hi);
                    for (int p = 0; p < params.oversample; ++p)
                        hi[p] = curves::apply (params.curve, hi[p], t);
                    wet = os[c]->downsample (hi);
                }

                const float dryDelayed = st.deltaDelay.process (driven);
                data[i] = (params.delta ? dryDelayed - wet : wet) * go;
            }
        }
    }

private:
    void applyPending() noexcept
    {
        if (pending.oversample != params.oversample)
            for (auto& c : chans)
            {
                c.os4.reset();
                c.os8.reset();
                c.comp.reset();
                c.deltaDelay.reset();
            }

        if (pending.monoLow >= Parameters::monoLowOffBelow)
        {
            const bool wasOff = params.monoLow < Parameters::monoLowOffBelow;
            if (wasOff)
                monoLow.reset();
            if (wasOff || ! juce::exactlyEqual (pending.monoLow, params.monoLow))
                monoLow.setFrequency (monoLowFreqHz (pending.monoLow));
        }

        if (! juce::exactlyEqual (pending.driveDb, params.driveDb))
            driveGain.setTargetValue (juce::Decibels::decibelsToGain (pending.driveDb));
        if (! juce::exactlyEqual (pending.outputDb, params.outputDb))
            outGain.setTargetValue (juce::Decibels::decibelsToGain (pending.outputDb));
        params = pending;
    }

    struct Channel
    {
        Oversampler os4 { 4 }, os8 { 8 };
        LatencyDelay comp;       // aligns the os-off wet path
        LatencyDelay deltaDelay; // aligns the driven dry path for delta
    };

    Parameters params, pending;
    MonoLow monoLow;
    std::array<Channel, 2> chans;
    juce::SmoothedValue<float> driveGain { 1.0f }, outGain { 1.0f };
};
} // namespace tagoclip
