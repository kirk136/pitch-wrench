// ─────────────────────────────────────────────────────────────────────────────
// PluginEditor.cpp  —  Pitch Wrench WebView editor implementation
// ─────────────────────────────────────────────────────────────────────────────

#include "PluginEditor.h"
#include "Parameters.h"
#include <PitchWrenchWebUIBinaryData.h>  // generato da CMake juce_add_binary_data

using namespace juce;
using namespace PitchWrench;

// ── Helper Resource Provider ──────────────────────────────────────────────────
static auto getResourceProvider() {
    return [](const juce::String& url) -> std::optional<juce::WebBrowserComponent::Resource> {
        const char* data = nullptr;
        int size = 0;
        juce::String mimeType = "text/html";
        
        if (url == "/" || url.endsWith("index.html")) {
            data = PitchWrenchWebUIBinaryData::index_html;
            size = PitchWrenchWebUIBinaryData::index_htmlSize;
            mimeType = "text/html";
        } else if (url.endsWith("style.css")) {
            data = PitchWrenchWebUIBinaryData::style_css;
            size = PitchWrenchWebUIBinaryData::style_cssSize;
            mimeType = "text/css";
        } else if (url.endsWith("app.js")) {
            data = PitchWrenchWebUIBinaryData::app_js;
            size = PitchWrenchWebUIBinaryData::app_jsSize;
            mimeType = "text/javascript";
        } else if (url.endsWith("logo.png")) {
            data = PitchWrenchWebUIBinaryData::logo_png;
            size = PitchWrenchWebUIBinaryData::logo_pngSize;
            mimeType = "image/png";
        }
        
        if (data != nullptr) {
            std::vector<std::byte> vec((const std::byte*)data, (const std::byte*)data + size);
            return juce::WebBrowserComponent::Resource { std::move(vec), mimeType };
        }
        return std::nullopt;
    };
}

// ── Constructor ───────────────────────────────────────────────────────────────
PitchWrenchEditor::PitchWrenchEditor(PitchWrenchProcessor& p)
    : AudioProcessorEditor(&p)
    , m_processor(p)
    , m_webView(WebBrowserComponent::Options{}
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withResourceProvider(getResourceProvider())
        .withNativeIntegrationEnabled()
        .withEventListener("hostSetParameter", [this](const juce::var& eventVar) {
            if (eventVar.isObject()) {
                auto* obj = eventVar.getDynamicObject();
                if (obj->hasProperty("param") && obj->hasProperty("value")) {
                    juce::String paramName = obj->getProperty("param").toString();
                    float value = obj->getProperty("value");
                    
                    juce::String paramID = "";
                    if (paramName == "semitones") paramID = ParamID::Semitones;
                    else if (paramName == "fineTune") paramID = ParamID::FineTune;
                    else if (paramName == "mix") paramID = ParamID::Mix;
                    else if (paramName == "enabled") paramID = ParamID::Enabled;
                    else if (paramName == "uiScale") paramID = ParamID::UiScale;

                    if (paramID.isNotEmpty()) {
                        if (auto* param = m_processor.getAPVTS().getParameter(paramID)) {
                            param->setValueNotifyingHost(param->convertTo0to1(value));
                        }
                    }
                }
            }
        })
        .withEventListener("ready", [this](const juce::var&) {
            m_webReady = true;
            updateUIParameter("semitones", m_processor.getCurrentSemitones());
            updateUIParameter("fineTune",  m_processor.getCurrentFineTune());
            updateUIParameter("mix",       m_processor.getCurrentMix());
            updateUIParameter("enabled",   m_processor.getAPVTS().getRawParameterValue(ParamID::Enabled)->load());
            updateUIParameter("uiScale",   m_processor.getAPVTS().getRawParameterValue(ParamID::UiScale)->load());
        }))
{
    // Dimensioni iniziali (Medium)
    setSize(kWidthMedium, kHeightMedium);

    addAndMakeVisible(m_webView);

    // Carica la UI
    m_webView.goToURL(juce::WebBrowserComponent::getResourceProviderRoot() + "index.html");

    // Ascolta i parametri APVTS per notificare la UI
    m_processor.getAPVTS().addParameterListener(ParamID::Semitones, this);
    m_processor.getAPVTS().addParameterListener(ParamID::FineTune,  this);
    m_processor.getAPVTS().addParameterListener(ParamID::Mix,       this);
    m_processor.getAPVTS().addParameterListener(ParamID::Enabled,   this);
    m_processor.getAPVTS().addParameterListener(ParamID::UiScale,   this);

    // Timer per sincronizzazione UI (polling ogni 33ms ≈ 30fps)
    startTimerHz(30);

    // Applica la scala UI iniziale
    const int initScale = static_cast<int>(
        m_processor.getAPVTS().getRawParameterValue(ParamID::UiScale)
            ->load(std::memory_order_relaxed));
    applyUiScale(initScale);
}

PitchWrenchEditor::~PitchWrenchEditor()
{
    stopTimer();
    m_processor.getAPVTS().removeParameterListener(ParamID::Semitones, this);
    m_processor.getAPVTS().removeParameterListener(ParamID::FineTune,  this);
    m_processor.getAPVTS().removeParameterListener(ParamID::Mix,       this);
    m_processor.getAPVTS().removeParameterListener(ParamID::Enabled,   this);
    m_processor.getAPVTS().removeParameterListener(ParamID::UiScale,   this);
}

// ── Layout ────────────────────────────────────────────────────────────────────
void PitchWrenchEditor::resized()
{
    m_webView.setBounds(getLocalBounds());
}

// ── UI Scale ─────────────────────────────────────────────────────────────────
void PitchWrenchEditor::applyUiScale(int scale)
{
    int w, h;
    switch (scale) {
        case 0:  w = kWidthSmall;  h = kHeightSmall;  break;
        case 2:  w = kWidthLarge;  h = kHeightLarge;  break;
        default: w = kWidthMedium; h = kHeightMedium; break;
    }
    setSize(w, h);
}

// ── APVTS Listener ────────────────────────────────────────────────────────────
void PitchWrenchEditor::parameterChanged(const String& paramID, float newValue)
{
    // Chiamato sul message thread — safe per UI
    if (paramID == ParamID::Semitones)
        updateUIParameter("semitones", newValue);
    else if (paramID == ParamID::FineTune)
        updateUIParameter("fineTune", newValue);
    else if (paramID == ParamID::Mix)
        updateUIParameter("mix", newValue);
    else if (paramID == ParamID::Enabled)
        updateUIParameter("enabled", newValue);
    else if (paramID == ParamID::UiScale) {
        applyUiScale(static_cast<int>(newValue));
        updateUIParameter("uiScale", newValue);
    }
}

// ── Timer callback (polling) ──────────────────────────────────────────────────
void PitchWrenchEditor::timerCallback()
{
    // Nessuna azione necessaria per ora —
    // le notifiche arrivano via parameterChanged().
    // Questo timer potrebbe essere usato per VU meter in futuro (v1.1).
}

// ── JS Communication ──────────────────────────────────────────────────────────
void PitchWrenchEditor::sendToUI(const String& js)
{
    if (m_webReady)
        m_webView.evaluateJavascript(js);
}

void PitchWrenchEditor::updateUIParameter(const String& paramName, float value)
{
    sendToUI("if(window.pitchWrench) pitchWrench.updateParam('"
             + paramName + "'," + String(value) + ");");
}
