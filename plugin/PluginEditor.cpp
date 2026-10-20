#include "PluginEditor.h"
#include "BinaryData.h"

#if JUCE_WINDOWS
 #include <windows.h>
#endif

namespace
{
// The approved mockup canvas. The web UI scales itself to any window on this
// aspect ratio (ui/src/fit.ts), so the editor can be resized freely instead of
// clipping when a host hands it a different size.
constexpr int    designWidth  = 620;
constexpr int    designHeight = 380;
constexpr double designRatio  = (double) designWidth / (double) designHeight;
constexpr int    minWidth     = 465;
constexpr int    maxWidth     = 1860;

const juce::Identifier editorWidthId { "editorWidth" };

int heightForWidth (int width) { return juce::roundToInt (width / designRatio); }

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


#if JUCE_WINDOWS
// WebView2 keeps a browser profile in its user data folder. The bare %TEMP%
// root is shared with every other app that does the same, so the plugin gets
// its own folder under %LOCALAPPDATA%. Hardening after a Cubase 12 / Windows 10
// crash on insert (report 19.10.2026); the crash cause itself is not proven.
static juce::File webViewDataFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::windowsLocalAppData)
                   .getChildFile ("TagoBeats")
                   .getChildFile ("TagoClip")
                   .getChildFile ("WebView2");
    dir.createDirectory();
    return dir;
}
#endif

#if JUCE_WINDOWS
// WebView2 takes Win32 keyboard focus on click and swallows every key, so the
// host never sees play/stop. The UI has no text input, so after each click it
// asks us to hand focus back to the editor's own HWND. JUCE forwards the keys
// it does not handle from there to the host window, the same path every
// native JUCE editor uses. Only acts while focus sits inside the web view.
static void releaseWebViewFocus (juce::Component& editor)
{
    if (auto* peer = editor.getPeer())
    {
        const auto peerWindow = (HWND) peer->getNativeHandle();
        const auto focused    = GetFocus();

        if (peerWindow != nullptr && focused != nullptr && focused != peerWindow
            && IsChild (peerWindow, focused))
            SetFocus (peerWindow);
    }
}
#endif

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
                           .withUserDataFolder (webViewDataFolder()))
#else
                   .withBackend (juce::WebBrowserComponent::Options::Backend::defaultBackend)
#endif
                   .withNativeIntegrationEnabled()
                   .withResourceProvider (lookupResource)
                   .withNativeFunction ("releaseFocus",
                                        [this] (const juce::Array<juce::var>&,
                                                juce::WebBrowserComponent::NativeFunctionCompletion done)
                                        {
#if JUCE_WINDOWS
                                            releaseWebViewFocus (*this);
#else
                                            (void) this;
#endif
                                            done (juce::var());
                                        })
                   .withOptionsFrom (driveRelay)
                   .withOptionsFrom (thresholdRelay)
                   .withOptionsFrom (curveRelay)
                   .withOptionsFrom (oversamplingRelay)
                   .withOptionsFrom (outputRelay)
                   .withOptionsFrom (monoLowRelay)
                   .withOptionsFrom (mixRelay)
                   .withOptionsFrom (deltaRelay)
                   .withOptionsFrom (bypassRelay)),
      driveAttachment (*p.apvts.getParameter (tagoclip::param::drive), driveRelay, nullptr),
      thresholdAttachment (*p.apvts.getParameter (tagoclip::param::threshold), thresholdRelay, nullptr),
      curveAttachment (*p.apvts.getParameter (tagoclip::param::curve), curveRelay, nullptr),
      oversamplingAttachment (*p.apvts.getParameter (tagoclip::param::oversampling), oversamplingRelay, nullptr),
      outputAttachment (*p.apvts.getParameter (tagoclip::param::output), outputRelay, nullptr),
      monoLowAttachment (*p.apvts.getParameter (tagoclip::param::monoLow), monoLowRelay, nullptr),
      mixAttachment (*p.apvts.getParameter (tagoclip::param::mix), mixRelay, nullptr),
      deltaAttachment (*p.apvts.getParameter (tagoclip::param::delta), deltaRelay, nullptr),
      bypassAttachment (*p.apvts.getParameter (tagoclip::param::bypass), bypassRelay, nullptr)
{
    addAndMakeVisible (browser);

#if TAGOCLIP_DEV_UI
    browser.goToURL ("http://localhost:5173");
#else
    browser.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
#endif

    // Resizable on a locked aspect ratio. Hosts that apply display scaling used
    // to get a window smaller than the UI and simply cut it off (reported
    // 24.09.2026 against TagoPitch for Cakewalk Sonar and Fender Studio Pro,
    // same fixed canvas here); a resizable editor lets the host negotiate a
    // size, and the UI scales into whatever it gets.
    setResizable (true, false);

    if (auto* constrainer = getConstrainer())
    {
        constrainer->setFixedAspectRatio (designRatio);
        constrainer->setSizeLimits (minWidth, heightForWidth (minWidth),
                                    maxWidth, heightForWidth (maxWidth));
    }

    const int savedWidth = (int) clipProcessor.apvts.state.getProperty (editorWidthId, designWidth);
    const int width      = juce::jlimit (minWidth, maxWidth, savedWidth);
    setSize (width, heightForWidth (width));

    startTimerHz (30);
}

void TagoClipEditor::timerCallback()
{
    auto* levels = new juce::DynamicObject();
    levels->setProperty ("in", clipProcessor.readInputPeak());
    levels->setProperty ("out", clipProcessor.readOutputPeak());
    levels->setProperty ("gr", clipProcessor.readGainReductionDb());
    browser.emitEventIfBrowserIsVisible ("levels", juce::var (levels));
}

void TagoClipEditor::resized()
{
    browser.setBounds (getLocalBounds());

    // Kept on the APVTS tree, so it rides along with getStateInformation and the
    // window comes back the size the user left it at.
    clipProcessor.apvts.state.setProperty (editorWidthId, getWidth(), nullptr);
}
