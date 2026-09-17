#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ClipEngine.h"

namespace tagoclip::param
{
// IDs are the contract between processor, the WebView UI (mockup/index.html)
// and the Python references in golden/make_reference.py. Never rename.
inline constexpr auto drive = "drive_db";            // -6..+24 dB
inline constexpr auto threshold = "threshold_steps"; // 1..127, t = v/128 (Fruity scale)
inline constexpr auto curve = "curve";               // 0 fl, 1 hard, 2 tanh
inline constexpr auto oversampling = "oversampling"; // 0 off, 1 4x, 2 8x
inline constexpr auto output = "output_db";          // -12..+12 dB
inline constexpr auto monoLow = "mono_low";          // 0..1 log sweep, < 0.03 off
inline constexpr auto mix = "mix";                   // 0..100 %, 100 = fully clipped
inline constexpr auto delta = "delta";
inline constexpr auto bypass = "bypass";
} // namespace tagoclip::param

class TagoClipProcessor : public juce::AudioProcessor
{
public:
    TagoClipProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;

    // Block peaks for the UI meters (in = pre chain, out = post gain).
    // Written on the audio thread, read by the editor's timer.
    float readInputPeak() noexcept { return inputPeak.exchange (0.0f, std::memory_order_relaxed); }
    float readOutputPeak() noexcept { return outputPeak.exchange (0.0f, std::memory_order_relaxed); }

    // Peak gain reduction of the last processed block, in dB (<= 0).
    float readGainReductionDb() const noexcept { return gainReductionDb.load (std::memory_order_relaxed); }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    tagoclip::Parameters currentParameters() const noexcept;

    void storePeak (std::atomic<float>& peak, float value) noexcept;

    juce::AudioParameterBool* bypassParam = nullptr;
    std::atomic<float>* driveRaw = nullptr;
    std::atomic<float>* thresholdRaw = nullptr;
    std::atomic<float>* curveRaw = nullptr;
    std::atomic<float>* osRaw = nullptr;
    std::atomic<float>* outputRaw = nullptr;
    std::atomic<float>* monoLowRaw = nullptr;
    std::atomic<float>* mixRaw = nullptr;
    std::atomic<float>* deltaRaw = nullptr;

    tagoclip::ClipEngine engine;

    std::atomic<float> inputPeak { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
    std::atomic<float> gainReductionDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TagoClipProcessor)
};
