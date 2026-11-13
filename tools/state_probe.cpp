// State round trip check for the processor: what getStateInformation writes has
// to come back identically, and a v1.0 state that predates the mix parameter has
// to load without dragging a stale mix value into the project.
//
// Written for the 14.09.2026 bug report ("everytime i close the plug in the
// settings go back to the default settings"), which could not be reproduced in
// FL. This makes the claim testable instead of inspected.
//
// Usage: TagoClipStateProbe   (prints one line per check, nonzero on failure)

#include <iostream>
#include <vector>

#include "../plugin/PluginProcessor.h"

namespace
{
struct Probe
{
    const char* id;
    float value; // denormalised, deliberately away from the default
};

// One distinct, non-default value per parameter in the contract.
const std::vector<Probe> probes {
    { tagoclip::param::drive, 9.3f },
    { tagoclip::param::threshold, 42.0f },
    { tagoclip::param::curve, 2.0f },        // tanh
    { tagoclip::param::oversampling, 1.0f }, // 4x
    { tagoclip::param::output, -3.4f },
    { tagoclip::param::monoLow, 0.62f },
    { tagoclip::param::mix, 37.5f },
    { tagoclip::param::delta, 1.0f },
    { tagoclip::param::bypass, 1.0f },
};

float readParam (TagoClipProcessor& p, const char* id)
{
    return p.apvts.getParameter (id)->convertFrom0to1 (p.apvts.getParameter (id)->getValue());
}

void writeParam (TagoClipProcessor& p, const char* id, float denormalised)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (denormalised));
}

bool report (bool ok, const juce::String& what)
{
    std::cout << (ok ? "PASS " : "FAIL ") << what << "\n";
    return ok;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    bool allOk = true;

    // 1. Round trip: a saved state has to come back on a fresh instance.
    juce::MemoryBlock saved;
    {
        TagoClipProcessor source;
        for (const auto& probe : probes)
            writeParam (source, probe.id, probe.value);
        source.getStateInformation (saved);
    }

    {
        TagoClipProcessor target;
        target.setStateInformation (saved.getData(), (int) saved.getSize());
        for (const auto& probe : probes)
        {
            const float got = readParam (target, probe.id);
            allOk &= report (std::abs (got - probe.value) < 1.0e-4f,
                             juce::String ("round trip ") + probe.id + ": wrote "
                                 + juce::String (probe.value, 3) + ", read "
                                 + juce::String (got, 3));
        }
        allOk &= report (saved.getSize() > 0, "state is not empty (" + juce::String ((int) saved.getSize()) + " bytes)");
    }

    // 2. Upgrade path: a v1.0 project has no mix entry. Recreate one by dropping
    //    the mix child from the saved tree, then load it into an instance whose
    //    mix was moved away from the default. Mix has to land on the default, not
    //    on whatever the instance happened to hold.
    {
        auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
        if (xml == nullptr)
            return report (false, "cannot parse the saved state"), 1;

        for (auto* child : xml->getChildIterator())
            if (child->getStringAttribute ("id") == tagoclip::param::mix)
            {
                xml->removeChildElement (child, true);
                break;
            }

        bool mixGone = true;
        for (auto* child : xml->getChildIterator())
            if (child->getStringAttribute ("id") == tagoclip::param::mix)
                mixGone = false;
        allOk &= report (mixGone, "built a v1.0 style state without a mix entry");

        juce::MemoryBlock legacy;
        juce::AudioProcessor::copyXmlToBinary (*xml, legacy);

        TagoClipProcessor target;
        writeParam (target, tagoclip::param::mix, 12.0f); // stale value from an earlier patch
        target.setStateInformation (legacy.getData(), (int) legacy.getSize());

        const float mix = readParam (target, tagoclip::param::mix);
        const float defaultMix = target.apvts.getParameter (tagoclip::param::mix)->convertFrom0to1 (
            target.apvts.getParameter (tagoclip::param::mix)->getDefaultValue());
        allOk &= report (std::abs (mix - defaultMix) < 1.0e-4f,
                         "v1.0 state loads mix at its default: expected "
                             + juce::String (defaultMix, 1) + ", got " + juce::String (mix, 1));

        // The rest of a v1.0 project still has to arrive untouched.
        for (const auto& probe : probes)
        {
            if (juce::String (probe.id) == tagoclip::param::mix)
                continue;
            const float got = readParam (target, probe.id);
            allOk &= report (std::abs (got - probe.value) < 1.0e-4f,
                             juce::String ("v1.0 state keeps ") + probe.id + ": "
                                 + juce::String (got, 3));
        }
    }

    // 3. Output gain routing (1.2): a state without the outputMode property comes
    //    from 1.1.x and has to load in legacy mode, a new instance and a new save
    //    use the wet-only mode, and the mode has to survive a round trip.
    {
        TagoClipProcessor fresh;
        allOk &= report (! fresh.isLegacyOutputMode(), "new instance uses wet-only output");

        juce::MemoryBlock newSave;
        fresh.getStateInformation (newSave);
        auto xml = juce::AudioProcessor::getXmlFromBinary (newSave.getData(), (int) newSave.getSize());
        allOk &= report (xml != nullptr && xml->getIntAttribute (TagoClipProcessor::outputModeProperty, -1) == 1,
                         "new save carries outputMode=1");

        TagoClipProcessor reloaded;
        reloaded.setStateInformation (newSave.getData(), (int) newSave.getSize());
        allOk &= report (! reloaded.isLegacyOutputMode(), "round trip keeps wet-only output");

        // Old state: the saved one with the property removed (what 1.1.x wrote).
        if (xml != nullptr)
        {
            xml->removeAttribute (TagoClipProcessor::outputModeProperty);
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary (*xml, old);

            TagoClipProcessor legacy;
            legacy.setStateInformation (old.getData(), (int) old.getSize());
            allOk &= report (legacy.isLegacyOutputMode(), "state without outputMode loads in legacy mode");

            juce::MemoryBlock resaved;
            legacy.getStateInformation (resaved);
            TagoClipProcessor again;
            again.setStateInformation (resaved.getData(), (int) resaved.getSize());
            allOk &= report (again.isLegacyOutputMode(), "round trip keeps legacy output");

            // Loading a new state into an instance that held a legacy one must flip it back.
            legacy.setStateInformation (newSave.getData(), (int) newSave.getSize());
            allOk &= report (! legacy.isLegacyOutputMode(), "loading a new state leaves legacy mode");
        }
    }

    std::cout << (allOk ? "\nall state checks passed\n" : "\nSTATE CHECKS FAILED\n");
    return allOk ? 0 : 1;
}
