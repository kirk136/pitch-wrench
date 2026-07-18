#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// PluginProcessor.h  —  JUCE AudioProcessor wrapper per Pitch Wrench
// Gestisce APVTS, bridge controlli → modulo, serializzazione preset.
// ─────────────────────────────────────────────────────────────────────────────

#include <JuceHeader.h>
#include "PitchWrenchModule.h"
#include "Parameters.h"

class PitchWrenchProcessor
    : public juce::AudioProcessor
    , public juce::AudioProcessorValueTreeState::Listener
{
public:
    PitchWrenchProcessor();
    ~PitchWrenchProcessor() override;

    // ── AudioProcessor ────────────────────────────────────────────────────────
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Pitch Wrench"; }
    bool   acceptsMidi()  const override { return false; }
    bool   producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    // ── Stato / Preset ────────────────────────────────────────────────────────
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ── Latenza ───────────────────────────────────────────────────────────────
    // Il modulo ha latenza algoritmica = 0. Riportiamo 0 al host.
    int getLatencySamples() { return m_module.getLatencySamples(); }

    // ── APVTS Listener ────────────────────────────────────────────────────────
    void parameterChanged(const juce::String& paramID, float newValue) override;

    // ── Accesso pubblico per l'editor ─────────────────────────────────────────
    juce::AudioProcessorValueTreeState& getAPVTS() { return m_apvts; }

    // Valore corrente (per la UI, non-realtime)
    float getCurrentSemitones() const { return m_module.getSemitonesRaw(); }
    float getCurrentFineTune() const { return m_module.getFineTuneRaw(); }
    float getCurrentMix() const { return m_module.getMixRaw(); }

private:
    // ── Modulo DSP ────────────────────────────────────────────────────────────
    PitchWrench::PitchWrenchModule m_module;

    // ── APVTS ─────────────────────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState m_apvts;

    // Puntatori raw ai parametri (performance: evita lookup by-string nel processBlock)
    std::atomic<float>* m_pSemitones = nullptr;
    std::atomic<float>* m_pFineTune  = nullptr;
    std::atomic<float>* m_pMix       = nullptr;
    std::atomic<float>* m_pEnabled   = nullptr;
    std::atomic<float>* m_pUiScale   = nullptr;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchWrenchProcessor)
};
