#include "PluginProcessor.h"

namespace
{
juce::String formatDb (float v, int)
{
    return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB";
}
} // namespace

TagoClipProcessor::TagoClipProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createLayout())
{
    using namespace tagoclip::param;
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (bypass));
    jassert (bypassParam != nullptr);
    driveRaw = apvts.getRawParameterValue (drive);
    thresholdRaw = apvts.getRawParameterValue (threshold);
    curveRaw = apvts.getRawParameterValue (curve);
    osRaw = apvts.getRawParameterValue (oversampling);
    outputRaw = apvts.getRawParameterValue (output);
    monoLowRaw = apvts.getRawParameterValue (monoLow);
    deltaRaw = apvts.getRawParameterValue (delta);
}

juce::AudioProcessorValueTreeState::ParameterLayout TagoClipProcessor::createLayout()
{
    using namespace tagoclip::param;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { drive, 1 }, "Drive",
        juce::NormalisableRange<float> (-6.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes {}.withStringFromValueFunction (formatDb)));

    // Fruity scale: integer steps, internally t = v/128 like the original.
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { threshold, 1 }, "Threshold", 1, 127, 100));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { curve, 1 }, "Curve",
        juce::StringArray { "Default", "Hard", "Tanh" }, 0));

    // Default off: the honest Fruity 1:1 mode (Robin, 15.07.2026). The UI
    // button cycles off -> 4x -> 8x -> off.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { oversampling, 1 }, "Oversampling",
        juce::StringArray { "Off", "4x", "8x" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { output, 1 }, "Output",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes {}.withStringFromValueFunction (formatDb)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { monoLow, 1 }, "Mono Low",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
        juce::AudioParameterFloatAttributes {}.withStringFromValueFunction (
            [] (float v, int)
            {
                if (v < tagoclip::Parameters::monoLowOffBelow)
                    return juce::String ("Off");
                return juce::String (juce::roundToInt (tagoclip::monoLowFreqHz (v))) + " Hz";
            })));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { delta, 1 }, "Delta", false));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { bypass, 1 }, "Bypass", false));

    return layout;
}

tagoclip::Parameters TagoClipProcessor::currentParameters() const noexcept
{
    tagoclip::Parameters p;
    p.curve = (tagoclip::curves::Type) juce::jlimit (0, 2, (int) curveRaw->load());
    p.thresholdSteps = juce::jlimit (1, 127, (int) thresholdRaw->load());
    p.driveDb = driveRaw->load();
    p.oversample = tagoclip::osFactorTable[juce::jlimit (0, 2, (int) osRaw->load())];
    p.outputDb = outputRaw->load();
    p.monoLow = monoLowRaw->load();
    p.delta = deltaRaw->load() > 0.5f;
    return p;
}

void TagoClipProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate, currentParameters());
    setLatencySamples (engine.latencySamples());
}

bool TagoClipProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void TagoClipProcessor::storePeak (std::atomic<float>& peak, float value) noexcept
{
    // Keep the max until the editor timer consumes it (exchange with 0).
    float current = peak.load (std::memory_order_relaxed);
    while (value > current
           && ! peak.compare_exchange_weak (current, value, std::memory_order_relaxed))
    {
    }
}

void TagoClipProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const float inPeak = buffer.getMagnitude (0, buffer.getNumSamples());
    storePeak (inputPeak, inPeak);

    if (bypassParam->get())
    {
        storePeak (outputPeak, inPeak);
        return;
    }

    engine.setParameters (currentParameters());
    engine.process (buffer);

    storePeak (outputPeak, buffer.getMagnitude (0, buffer.getNumSamples()));
}

void TagoClipProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TagoClipProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* TagoClipProcessor::createEditor()
{
    // Placeholder until the WebView port of mockup/index.html lands.
    return new juce::GenericAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TagoClipProcessor();
}
