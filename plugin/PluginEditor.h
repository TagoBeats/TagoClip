#pragma once

#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

class TagoClipEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit TagoClipEditor (TagoClipProcessor&);

    void resized() override;

private:
    // Pushes the block peaks from the processor to the UI meters (~30 Hz).
    void timerCallback() override;

    TagoClipProcessor& clipProcessor;
    static std::optional<juce::WebBrowserComponent::Resource> lookupResource (const juce::String& url);

    // Relays bridge WebView controls to APVTS parameters; their names are the
    // IDs the frontend queries via getSliderState()/getToggleState().
    juce::WebSliderRelay driveRelay { tagoclip::param::drive };
    juce::WebSliderRelay thresholdRelay { tagoclip::param::threshold };
    juce::WebSliderRelay curveRelay { tagoclip::param::curve };
    juce::WebSliderRelay oversamplingRelay { tagoclip::param::oversampling };
    juce::WebSliderRelay outputRelay { tagoclip::param::output };
    juce::WebSliderRelay monoLowRelay { tagoclip::param::monoLow };
    juce::WebSliderRelay mixRelay { tagoclip::param::mix };
    juce::WebToggleButtonRelay deltaRelay { tagoclip::param::delta };
    juce::WebToggleButtonRelay bypassRelay { tagoclip::param::bypass };

    juce::WebBrowserComponent browser;

    juce::WebSliderParameterAttachment driveAttachment;
    juce::WebSliderParameterAttachment thresholdAttachment;
    juce::WebSliderParameterAttachment curveAttachment;
    juce::WebSliderParameterAttachment oversamplingAttachment;
    juce::WebSliderParameterAttachment outputAttachment;
    juce::WebSliderParameterAttachment monoLowAttachment;
    juce::WebSliderParameterAttachment mixAttachment;
    juce::WebToggleButtonParameterAttachment deltaAttachment;
    juce::WebToggleButtonParameterAttachment bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TagoClipEditor)
};
