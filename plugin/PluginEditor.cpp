#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
const char* mimeForExtension (const juce::String& ext)
{
    if (ext == "html") return "text/html";
    if (ext == "js")   return "text/javascript";
    if (ext == "css")  return "text/css";
    if (ext == "svg")  return "image/svg+xml";
    if (ext == "json") return "application/json";
    if (ext == "png")  return "image/png";
    if (ext == "woff2") return "font/woff2";
    return "application/octet-stream";
}
} // namespace

std::optional<juce::WebBrowserComponent::Resource> TagoClipEditor::lookupResource (const juce::String& url)
{
    // The Vite build is bundled as a zip in BinaryData; entries are stored
    // relative to dist/ (index.html, assets/...).
    static juce::MemoryInputStream zipStream (BinaryData::webui_zip, BinaryData::webui_zipSize, false);
    static juce::ZipFile zip (zipStream);

    const auto path = url == "/" ? juce::String ("index.html") : url.fromFirstOccurrenceOf ("/", false, false);

    if (const auto* entry = zip.getEntry (path))
    {
        std::unique_ptr<juce::InputStream> stream (zip.createStreamForEntry (*entry));
        if (stream == nullptr)
            return std::nullopt;

        std::vector<std::byte> data ((size_t) stream->getTotalLength());
        stream->read (data.data(), (int) data.size());
        return juce::WebBrowserComponent::Resource { std::move (data),
                                                     mimeForExtension (path.fromLastOccurrenceOf (".", false, false)) };
    }
    return std::nullopt;
}

TagoClipEditor::TagoClipEditor (TagoClipProcessor& p)
    : AudioProcessorEditor (p),
      clipProcessor (p),
      browser (juce::WebBrowserComponent::Options {}
                   // Windows must opt in to WebView2; the default there is the legacy
                   // IE engine, which cannot run the React UI. macOS default is WKWebView.
#if JUCE_WINDOWS
                   .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                   .withWinWebView2Options (
                       juce::WebBrowserComponent::Options::WinWebView2 {}
                           .withUserDataFolder (juce::File::getSpecialLocation (
                               juce::File::SpecialLocationType::tempDirectory)))
#else
                   .withBackend (juce::WebBrowserComponent::Options::Backend::defaultBackend)
#endif
                   .withNativeIntegrationEnabled()
                   .withResourceProvider (lookupResource)
                   .withOptionsFrom (driveRelay)
                   .withOptionsFrom (thresholdRelay)
                   .withOptionsFrom (curveRelay)
                   .withOptionsFrom (oversamplingRelay)
                   .withOptionsFrom (outputRelay)
                   .withOptionsFrom (monoLowRelay)
                   .withOptionsFrom (deltaRelay)
                   .withOptionsFrom (bypassRelay)),
      driveAttachment (*p.apvts.getParameter (tagoclip::param::drive), driveRelay, nullptr),
      thresholdAttachment (*p.apvts.getParameter (tagoclip::param::threshold), thresholdRelay, nullptr),
      curveAttachment (*p.apvts.getParameter (tagoclip::param::curve), curveRelay, nullptr),
      oversamplingAttachment (*p.apvts.getParameter (tagoclip::param::oversampling), oversamplingRelay, nullptr),
      outputAttachment (*p.apvts.getParameter (tagoclip::param::output), outputRelay, nullptr),
      monoLowAttachment (*p.apvts.getParameter (tagoclip::param::monoLow), monoLowRelay, nullptr),
      deltaAttachment (*p.apvts.getParameter (tagoclip::param::delta), deltaRelay, nullptr),
      bypassAttachment (*p.apvts.getParameter (tagoclip::param::bypass), bypassRelay, nullptr)
{
    addAndMakeVisible (browser);

#if TAGOCLIP_DEV_UI
    browser.goToURL ("http://localhost:5173");
#else
    browser.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
#endif

    // Mockup canvas is 620x380 (see mockup/index.html).
    setSize (620, 380);

    startTimerHz (30);
}

void TagoClipEditor::timerCallback()
{
    auto* levels = new juce::DynamicObject();
    levels->setProperty ("in", clipProcessor.readInputPeak());
    levels->setProperty ("out", clipProcessor.readOutputPeak());
    browser.emitEventIfBrowserIsVisible ("levels", juce::var (levels));
}

void TagoClipEditor::resized()
{
    browser.setBounds (getLocalBounds());
}
