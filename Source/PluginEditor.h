#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// PluginEditor.h  —  Pitch Wrench JUCE WebView Editor
// Carica WebUI/ (bundled come BinaryData) in un WebBrowserComponent.
// Comunicazione bidirezionale C++ ↔ JavaScript.
// ─────────────────────────────────────────────────────────────────────────────

#include <JuceHeader.h>
#include "PluginProcessor.h"

class PitchWrenchEditor
    : public juce::AudioProcessorEditor
    , private juce::AudioProcessorValueTreeState::Listener
    , private juce::Timer
{
public:
    explicit PitchWrenchEditor(PitchWrenchProcessor& processor);
    ~PitchWrenchEditor() override;

    void paint(juce::Graphics&) override {}
    void resized() override;

private:
    // ── APVTS Listener ────────────────────────────────────────────────────────
    void parameterChanged(const juce::String& paramID, float newValue) override;

    // ── Timer per polling (aggiornamento UI ogni ~30ms) ───────────────────────
    void timerCallback() override;

    // ── Comunicazione verso JS ────────────────────────────────────────────────
    void sendToUI(const juce::String& js);
    void updateUIParameter(const juce::String& paramName, float value);

    // ── Dimensioni editor per ogni UI Scale ───────────────────────────────────
    static constexpr int kWidthSmall  = 480;
    static constexpr int kHeightSmall = 320;
    static constexpr int kWidthMedium = 640;
    static constexpr int kHeightMedium= 426;
    static constexpr int kWidthLarge  = 800;
    static constexpr int kHeightLarge = 534;

    void applyUiScale(int scale);

    // ── Members ───────────────────────────────────────────────────────────────
    PitchWrenchProcessor& m_processor;
    juce::WebBrowserComponent m_webView;

    bool m_webReady = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchWrenchEditor)
};
