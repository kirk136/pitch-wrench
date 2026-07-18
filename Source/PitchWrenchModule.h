#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// PitchWrenchModule.h  —  Platform::IModule implementation
// Zero JUCE. Thread-safe parameters via std::atomic.
// ─────────────────────────────────────────────────────────────────────────────

#include "PlatformModule.h"
#include "PitchWrenchDSP.h"
#include "Parameters.h"
#include <atomic>
#include <cstring>

namespace PitchWrench {

// ── Stato serializzabile ──────────────────────────────────────────────────────
struct ModuleState {
    uint32_t stateVersion = 2; // version bumped to 2 for new parameters
    float    semitones    = 0.0f;
    float    fineTune     = 0.0f;
    float    mix          = 1.0f;
    bool     enabled      = true;
    int      uiScale      = 1;   // 0=Small, 1=Medium, 2=Large
};

// ─────────────────────────────────────────────────────────────────────────────
class PitchWrenchModule : public Platform::IModule {
public:
    PitchWrenchModule()  = default;
    ~PitchWrenchModule() = default;

    // ── Descriptor ────────────────────────────────────────────────────────────
    Platform::ModuleDescriptor getDescriptor() const override {
        return {
            "pulverine.pitchwrench",
            "Pitch Wrench",
            "1.0.0",
            "Pitch",
            "Pulverine Audio"
        };
    }

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    void initialize() override {}  // Tutte le init in prepare()

    void prepare(double sampleRate, int maxBlockSize) override {
        m_dsp.prepare(sampleRate, maxBlockSize);
        m_dsp.setPitchSemitones(m_semitones.load(std::memory_order_relaxed));
        m_dsp.setFineTune(m_fineTune.load(std::memory_order_relaxed));
        m_dsp.setMix(m_mix.load(std::memory_order_relaxed));
        m_dsp.setEnabled(m_enabled.load(std::memory_order_relaxed) >= 0.5f);
        m_prepared = true;
    }

    void reset() override {
        m_dsp.reset();
    }

    void suspend() override  {}
    void resume() override   {}

    void shutdown() override {
        m_prepared = false;
        m_dsp.reset();
    }

    // ── Process — REALTIME SAFE ───────────────────────────────────────────────
    void process(const float* const* inputs,
                 float* const*       outputs,
                 int numInputChannels,
                 int numOutputChannels,
                 int numSamples) override
    {
        if (!m_prepared) return;

        // Sync parametri atomici → DSP (lettura relaxed: ok, DSP smoother inside)
        m_dsp.setPitchSemitones(m_semitones.load(std::memory_order_relaxed));
        m_dsp.setFineTune(m_fineTune.load(std::memory_order_relaxed));
        m_dsp.setMix(m_mix.load(std::memory_order_relaxed));
        m_dsp.setEnabled(m_enabled.load(std::memory_order_relaxed) >= 0.5f);

        // Processa il primo canale
        if (numInputChannels > 0 && numOutputChannels > 0) {
            m_dsp.process(inputs[0], outputs[0], numSamples);
        }

        // Se stereo: processa anche il canale destro (indipendente)
        // In questa versione MVP copiamo il risultato del L su R (mono→stereo OK)
        if (numOutputChannels > 1 && numInputChannels > 0) {
            const size_t bytes = sizeof(float) * static_cast<size_t>(numSamples);
            std::memcpy(outputs[1], outputs[0], bytes);
        }
    }

    // ── Parametri ─────────────────────────────────────────────────────────────
    void setParameter(int paramIndex, float normalizedValue) override {
        switch (paramIndex) {
            case kSemitones:
                m_semitones.store(
                    denormalizeSemitones(normalizedValue),
                    std::memory_order_relaxed);
                break;
            case kFineTune:
                m_fineTune.store(
                    denormalizeFineTune(normalizedValue),
                    std::memory_order_relaxed);
                break;
            case kMix:
                m_mix.store(
                    denormalizeMix(normalizedValue),
                    std::memory_order_relaxed);
                break;
            case kEnabled:
                m_enabled.store(
                    (normalizedValue >= 0.5f) ? 1.0f : 0.0f,
                    std::memory_order_relaxed);
                break;
            case kUiScale:
                m_uiScale.store(
                    std::round(normalizedValue * 2.0f),
                    std::memory_order_relaxed);
                break;
            default: break;
        }
    }

    float getParameter(int paramIndex) const override {
        switch (paramIndex) {
            case kSemitones: return normalizeSemitones(
                                 m_semitones.load(std::memory_order_relaxed));
            case kFineTune:  return normalizeFineTune(
                                 m_fineTune.load(std::memory_order_relaxed));
            case kMix:       return normalizeMix(
                                 m_mix.load(std::memory_order_relaxed));
            case kEnabled:   return m_enabled.load(std::memory_order_relaxed);
            case kUiScale:   return m_uiScale.load(std::memory_order_relaxed) / 2.0f;
            default:         return 0.0f;
        }
    }

    // Accesso diretto non normalizzato (per il wrapper JUCE)
    float getSemitonesRaw() const {
        return m_semitones.load(std::memory_order_relaxed);
    }
    float getFineTuneRaw() const {
        return m_fineTune.load(std::memory_order_relaxed);
    }
    float getMixRaw() const {
        return m_mix.load(std::memory_order_relaxed);
    }

    int getLatencySamples() const {
        return m_dsp.getLatencySamples();  // = 0
    }

    // ── Stato serializzabile ──────────────────────────────────────────────────
    void saveState(std::vector<uint8_t>& dest) override {
        ModuleState s;
        s.semitones = m_semitones.load(std::memory_order_relaxed);
        s.fineTune  = m_fineTune.load(std::memory_order_relaxed);
        s.mix       = m_mix.load(std::memory_order_relaxed);
        s.enabled   = m_enabled.load(std::memory_order_relaxed) >= 0.5f;
        s.uiScale   = static_cast<int>(m_uiScale.load(std::memory_order_relaxed));

        dest.resize(sizeof(ModuleState));
        std::memcpy(dest.data(), &s, sizeof(ModuleState));
    }

    void loadState(const std::vector<uint8_t>& source) override {
        if (source.size() < sizeof(ModuleState)) return;

        ModuleState s;
        std::memcpy(&s, source.data(), sizeof(ModuleState));

        if (s.stateVersion == 1) {
            // Migrazione da v1
            s.fineTune = 0.0f;
            s.mix = 1.0f;
        } else if (s.stateVersion != 2) {
            return; // versione non supportata
        }

        // Validazione range
        s.semitones = std::clamp(s.semitones, ParamRange::SemitonesMin, ParamRange::SemitonesMax);
        s.fineTune  = std::clamp(s.fineTune, ParamRange::FineTuneMin, ParamRange::FineTuneMax);
        s.mix       = std::clamp(s.mix, ParamRange::MixMin, ParamRange::MixMax);
        s.uiScale   = std::clamp(s.uiScale, 0, 2);

        m_semitones.store(s.semitones, std::memory_order_relaxed);
        m_fineTune.store(s.fineTune, std::memory_order_relaxed);
        m_mix.store(s.mix, std::memory_order_relaxed);
        m_enabled.store(s.enabled ? 1.0f : 0.0f, std::memory_order_relaxed);
        m_uiScale.store(static_cast<float>(s.uiScale), std::memory_order_relaxed);
    }

private:
    PitchWrenchDSP m_dsp;

    // Thread-safe parameters (scritti dal control thread, letti dall'audio thread)
    std::atomic<float> m_semitones { 0.0f };
    std::atomic<float> m_fineTune  { 0.0f };
    std::atomic<float> m_mix       { 1.0f };
    std::atomic<float> m_enabled   { 1.0f };
    std::atomic<float> m_uiScale   { 1.0f };

    bool m_prepared = false;
};

} // namespace PitchWrench
