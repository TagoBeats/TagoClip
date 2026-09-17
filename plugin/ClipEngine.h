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
    float mix = 1.0f;         // 0 = raw input, 1 = fully clipped; ignored in delta mode
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
//      removed, in positive polarity)
//   -> mix (blend against the raw plugin input, skipped in delta mode)
//   -> output gain
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
            c.dryDelay.reset();
        }

        driveGain.reset (sampleRate, 0.02);
        outGain.reset (sampleRate, 0.02);
        mixAmount.reset (sampleRate, 0.02);
        driveGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.driveDb));
        outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.outputDb));
        mixAmount.setCurrentAndTargetValue (params.mix);
    }

    void setParameters (const Parameters& p) noexcept { pending = p; }

    int latencySamples() const noexcept { return Oversampler::totalLatency; }

    // Peak gain reduction of the last block, in dB and never above 0. Measured
    // across the curve alone (driven in, clipped out), so drive, mix, delta and
    // the output trim cannot flatter the number. The curve is monotonic in |x|,
    // so the loudest driven sample is also the loudest clipped one and the block
    // peak ratio is the true reduction at that sample.
    float lastGainReductionDb() const noexcept { return grDb; }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        applyPending();

        const int n = buffer.getNumSamples();
        const int numCh = juce::jmin ((int) chans.size(), buffer.getNumChannels());
        float* chan[2] = { buffer.getWritePointer (0),
                           numCh > 1 ? buffer.getWritePointer (1) : nullptr };

        // Mono-low runs inside the sample loop now, so the raw input is still
        // available as the dry side of the mix (it is pre mono-low by design).
        const bool useMonoLow =
            chan[1] != nullptr && params.monoLow >= Parameters::monoLowOffBelow;

        const double t = params.thresholdSteps / 128.0;
        const bool useOs = params.oversample != 1;
        float peakDriven = 0.0f, peakWet = 0.0f;
        Oversampler* os[2] = {};
        for (int c = 0; c < numCh; ++c)
            os[c] = params.oversample == 4 ? &chans[(size_t) c].os4 : &chans[(size_t) c].os8;

        for (int i = 0; i < n; ++i)
        {
            const float gd = driveGain.getNextValue();
            const float go = outGain.getNextValue();
            const float gm = mixAmount.getNextValue();

            const float dry[2] = { chan[0][i], chan[1] != nullptr ? chan[1][i] : 0.0f };

            if (useMonoLow)
                monoLow.processSample (chan[0][i], chan[1][i]);

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

                peakDriven = juce::jmax (peakDriven, std::abs (driven));
                peakWet = juce::jmax (peakWet, std::abs (wet));

                // Both delay lines are fed every sample so switching delta or
                // mix never reads a stale line.
                const float drivenDelayed = st.deltaDelay.process (driven);
                const float dryDelayed = st.dryDelay.process (dry[c]);
                const float blended = params.delta ? drivenDelayed - wet
                                                   : gm * wet + (1.0f - gm) * dryDelayed;
                data[i] = blended * go;
            }
        }

        // Silence reads as no reduction, not as a division by a noise floor.
        grDb = peakDriven > 1.0e-5f
                 ? juce::jmin (0.0f, juce::Decibels::gainToDecibels (peakWet / peakDriven, -60.0f))
                 : 0.0f;
    }

private:
    void applyPending() noexcept
    {
        // dryDelay is deliberately not reset here: it carries the untouched
        // input, which the resampler switch cannot invalidate, so the dry side
        // of the mix stays clickless while the wet path restarts.
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
        if (! juce::exactlyEqual (pending.mix, params.mix))
            mixAmount.setTargetValue (pending.mix);
        params = pending;
    }

    struct Channel
    {
        Oversampler os4 { 4 }, os8 { 8 };
        LatencyDelay comp;       // aligns the os-off wet path
        LatencyDelay deltaDelay; // aligns the driven dry path for delta
        LatencyDelay dryDelay;   // aligns the raw input for the mix blend
    };

    Parameters params, pending;
    float grDb = 0.0f; // written on the audio thread, copied out by the processor
    MonoLow monoLow;
    std::array<Channel, 2> chans;
    juce::SmoothedValue<float> driveGain { 1.0f }, outGain { 1.0f }, mixAmount { 1.0f };
};
} // namespace tagoclip
