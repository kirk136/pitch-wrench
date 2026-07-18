// ─────────────────────────────────────────────────────────────────────────────
// PluginProcessor.cpp  —  Pitch Wrench AudioProcessor implementation
// ─────────────────────────────────────────────────────────────────────────────

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

using namespace PitchWrench;
using namespace juce;

// ── Parameter layout ──────────────────────────────────────────────────────────
AudioProcessorValueTreeState::ParameterLayout
PitchWrenchProcessor::createParameterLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    // Semitones: -24 to +24, step 1, default 0
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ParamID::Semitones, 1},
        "Semitones",
        NormalisableRange<float>(-24.0f, 24.0f, 1.0f),
        0.0f,
        AudioParameterFloatAttributes{}
            .withLabel("st")
            .withStringFromValueFunction([](float v, int) -> String {
                return (v >= 0 ? "+" : "") + String(static_cast<int>(v)) + " st";
            })
    ));

    // FineTune: -100 to +100 cents
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ParamID::FineTune, 1},
        "Fine-Tune",
        NormalisableRange<float>(ParamRange::FineTuneMin, ParamRange::FineTuneMax, 1.0f),
        ParamRange::FineTuneDefault,
        AudioParameterFloatAttributes{}
            .withLabel("cents")
            .withStringFromValueFunction([](float v, int) -> String {
                return (v > 0 ? "+" : "") + String(static_cast<int>(v)) + " ct";
            })
    ));

    // Mix: 0 to 1 (0 to 100%)
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ParamID::Mix, 1},
        "Mix",
        NormalisableRange<float>(ParamRange::MixMin, ParamRange::MixMax, 0.01f),
        ParamRange::MixDefault,
        AudioParameterFloatAttributes{}
            .withLabel("%")
            .withStringFromValueFunction([](float v, int) -> String {
                return String(static_cast<int>(v * 100.0f)) + " %";
            })
    ));

    // Enabled (bypass)
    layout.add(std::make_unique<AudioParameterBool>(
        ParameterID{ParamID::Enabled, 1},
        "Enabled",
        true
    ));

    // UI Scale: Small (0), Medium (1), Large (2)
    layout.add(std::make_unique<AudioParameterChoice>(
        ParameterID{ParamID::UiScale, 1},
        "UI Scale",
        StringArray{"Small", "Medium", "Large"},
        1  // default: Medium
    ));

    return layout;
}

// ── Constructor ───────────────────────────────────────────────────────────────
PitchWrenchProcessor::PitchWrenchProcessor()
    : AudioProcessor(BusesProperties()
        .withInput ("Input",  AudioChannelSet::stereo(), true)
        .withOutput("Output", AudioChannelSet::stereo(), true))
    , m_apvts(*this, nullptr, "PitchWrenchState", createParameterLayout())
{
    // Registra listener per tutti i parametri
    m_apvts.addParameterListener(ParamID::Semitones, this);
    m_apvts.addParameterListener(ParamID::FineTune,  this);
    m_apvts.addParameterListener(ParamID::Mix,       this);
    m_apvts.addParameterListener(ParamID::Enabled,   this);
    m_apvts.addParameterListener(ParamID::UiScale,   this);

    // Puntatori raw (evita lookup by-string nel processBlock)
    m_pSemitones = m_apvts.getRawParameterValue(ParamID::Semitones);
    m_pFineTune  = m_apvts.getRawParameterValue(ParamID::FineTune);
    m_pMix       = m_apvts.getRawParameterValue(ParamID::Mix);
    m_pEnabled   = m_apvts.getRawParameterValue(ParamID::Enabled);
    m_pUiScale   = m_apvts.getRawParameterValue(ParamID::UiScale);

    m_module.initialize();
}

PitchWrenchProcessor::~PitchWrenchProcessor()
{
    m_apvts.removeParameterListener(ParamID::Semitones, this);
    m_apvts.removeParameterListener(ParamID::FineTune,  this);
    m_apvts.removeParameterListener(ParamID::Mix,       this);
    m_apvts.removeParameterListener(ParamID::Enabled,   this);
    m_apvts.removeParameterListener(ParamID::UiScale,   this);
    m_module.shutdown();
}

// ── Prepare ───────────────────────────────────────────────────────────────────
void PitchWrenchProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    m_module.prepare(sampleRate, samplesPerBlock);
    setLatencySamples(m_module.getLatencySamples());  // = 0

    // Sync parametri iniziali
    m_module.setParameter(kSemitones,
        normalizeSemitones(m_pSemitones->load(std::memory_order_relaxed)));
    m_module.setParameter(kFineTune,
        normalizeFineTune(m_pFineTune->load(std::memory_order_relaxed)));
    m_module.setParameter(kMix,
        normalizeMix(m_pMix->load(std::memory_order_relaxed)));
    m_module.setParameter(kEnabled,
        m_pEnabled->load(std::memory_order_relaxed));
    m_module.setParameter(kUiScale,
        m_pUiScale->load(std::memory_order_relaxed) / 2.0f);
}

void PitchWrenchProcessor::releaseResources()
{
    m_module.suspend();
}

// ── Process Block — REALTIME SAFE ─────────────────────────────────────────────
void PitchWrenchProcessor::processBlock(AudioBuffer<float>& buffer,
                                         MidiBuffer& /*midi*/)
{
    ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n      = buffer.getNumSamples();

    // Azzera canali di output non usati
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear(ch, 0, n);

    // Sync parametri (lettura atomic, senza lock, come in PaperCUT)
    m_module.setParameter(kSemitones,
        normalizeSemitones(m_pSemitones->load(std::memory_order_relaxed)));
    m_module.setParameter(kFineTune,
        normalizeFineTune(m_pFineTune->load(std::memory_order_relaxed)));
    m_module.setParameter(kMix,
        normalizeMix(m_pMix->load(std::memory_order_relaxed)));
    m_module.setParameter(kEnabled,
        m_pEnabled->load(std::memory_order_relaxed));

    // Prepara array di puntatori per IModule::process()
    const float* inputs[2]  = {
        numIn > 0 ? buffer.getReadPointer(0) : nullptr,
        numIn > 1 ? buffer.getReadPointer(1) : nullptr
    };
    float* outputs[2] = {
        numOut > 0 ? buffer.getWritePointer(0) : nullptr,
        numOut > 1 ? buffer.getWritePointer(1) : nullptr
    };

    m_module.process(inputs, outputs, numIn, numOut, n);
}

// ── Parameter Listener (message thread) ──────────────────────────────────────
void PitchWrenchProcessor::parameterChanged(const String& /*paramID*/,
                                              float /*newValue*/)
{
    // I parametri vengono sincronizzati direttamente in processBlock()
    // via atomic raw pointer. Nessuna azione necessaria qui.
    // Potremmo notificare l'editor di aggiornarsi, ma APVTS
    // gestisce già questo automaticamente.
}

// ── Stato / Preset ────────────────────────────────────────────────────────────
void PitchWrenchProcessor::getStateInformation(MemoryBlock& destData)
{
    auto state = m_apvts.copyState();
    auto xml   = state.createXml();
    if (xml) copyXmlToBinary(*xml, destData);
}

void PitchWrenchProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml && xml->hasTagName(m_apvts.state.getType()))
        m_apvts.replaceState(ValueTree::fromXml(*xml));
}

// ── Editor ────────────────────────────────────────────────────────────────────
AudioProcessorEditor* PitchWrenchProcessor::createEditor()
{
    return new PitchWrenchEditor(*this);
}

// ── JUCE Plugin factory ───────────────────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PitchWrenchProcessor();
}
