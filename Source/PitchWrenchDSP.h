#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// PitchWrenchDSP.h  —  Pitch Wrench DSP Engine
// Vanilla C++17. Zero JUCE. Zero allocazioni in process().
//
// Algoritmo: Dual-Head Crossfading Delay Line
//   Due testine di lettura su un buffer circolare. Mentre una testa legge
//   a velocità pitchRatio, l'altra la sostituisce con un crossfade Hann
//   quando si avvicina alla zona di collisione con il write pointer.
//   Latenza algoritmica: 0 campioni.
//   Interpolazione: Hermite cubica (qualità/costo ottimale per chitarra).
//
// Riferimento sonoro: DigiTech Whammy / Eventide H90 style.
// ─────────────────────────────────────────────────────────────────────────────

#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>

namespace PitchWrench {

// ── Costanti ──────────────────────────────────────────────────────────────────
static constexpr float kMaxBufferMs       = 35.0f;    // 35ms massima latenza/finestra
static constexpr int   kMaxSampleRate     = 192000;
static constexpr int   kMaxBufferSamples  = static_cast<int>(kMaxBufferMs * 192.0f) + 100;
static constexpr float kSmoothTimeMs      = 8.0f;     // smoothing pitch ratio (ms)
static constexpr float kBypassFadeMs      = 3.0f;     // crossfade bypass (ms)

// ─────────────────────────────────────────────────────────────────────────────
class PitchWrenchDSP {
public:

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    void prepare(double sampleRate, int maxBlockSize)
    {
        m_sampleRate = sampleRate;

        // Alloca buffer circolare — unica allocazione consentita
        const int bufSize = static_cast<int>(sampleRate * kMaxBufferMs * 0.001f) + 8;
        m_buffer.assign(bufSize, 0.0f);
        m_bufferSize = bufSize;

        // Coefficiente smoothing one-pole per il pitch ratio
        const float smoothSamples = static_cast<float>(sampleRate * kSmoothTimeMs * 0.001);
        m_smoothCoeff = std::exp(-1.0f / smoothSamples);

        // Coefficiente smoothing per bypass fade
        const float bypassSamples = static_cast<float>(sampleRate * kBypassFadeMs * 0.001);
        m_bypassSmoothCoeff = std::exp(-1.0f / bypassSamples);

        reset();
        m_prepared = true;
    }

    void reset()
    {
        std::fill(m_buffer.begin(), m_buffer.end(), 0.0f);
        m_writePos = 0;
        m_phase = 0.0f;

        m_currentPitchRatio = 1.0f;
        m_bypassGain = 1.0f;
    }

    // ── Parametri ─────────────────────────────────────────────────────────────
    void setPitchSemitones(float semitones)
    {
        m_semitones = semitones;
        updatePitchRatio();
    }

    void setFineTune(float cents)
    {
        m_fineTuneCents = cents;
        updatePitchRatio();
    }

    void setMix(float mix)
    {
        m_targetMix = std::clamp(mix, 0.0f, 1.0f);
    }

    void setEnabled(bool enabled)
    {
        m_targetBypassGain = enabled ? 1.0f : 0.0f;
    }

    int getLatencySamples() const { return 0; }

private:
    void updatePitchRatio()
    {
        // pitch ratio = 2^((semitones + cents/100) / 12)
        const float totalSemitones = m_semitones + (m_fineTuneCents / 100.0f);
        m_targetPitchRatio = std::pow(2.0f, totalSemitones / 12.0f);
    }

    // ── Process — REALTIME SAFE ───────────────────────────────────────────────
    // Nessuna allocazione. Nessun lock. Nessun I/O. Deterministico.
public:
    void process(const float* input, float* output, int numSamples)
    {
        if (!m_prepared) {
            std::memset(output, 0, sizeof(float) * numSamples);
            return;
        }

        const float windowSamples = static_cast<float>(m_sampleRate * kMaxBufferMs * 0.001f);
        const float minDelaySamples = 4.0f; // Safe margin for cubic interpolation

        for (int i = 0; i < numSamples; ++i) {
            // 1. Write current sample to circular buffer
            m_buffer[m_writePos] = input[i];

            // 2. Smooth pitch ratio
            m_currentPitchRatio = m_smoothCoeff * m_currentPitchRatio
                                 + (1.0f - m_smoothCoeff) * m_targetPitchRatio;

            // 3. Calculate LFO rate and phase increment
            const float rate = 1.0f - m_currentPitchRatio;
            const float phaseInc = std::abs(rate) / windowSamples;

            m_phase += phaseInc;
            if (m_phase >= 1.0f) m_phase -= 1.0f;

            const float phase1 = m_phase;
            float phase2 = m_phase + 0.5f;
            if (phase2 >= 1.0f) phase2 -= 1.0f;

            // 4. Calculate delays for both taps
            float delay1, delay2;
            if (rate >= 0.0f) { // Pitch Down or Unison
                delay1 = minDelaySamples + phase1 * windowSamples;
                delay2 = minDelaySamples + phase2 * windowSamples;
            } else {            // Pitch Up (sawtooth goes downwards)
                delay1 = minDelaySamples + (1.0f - phase1) * windowSamples;
                delay2 = minDelaySamples + (1.0f - phase2) * windowSamples;
            }

            // 5. Constant-power crossfade con derivata zero ai bordi (elimina il "thump" dell'elicottero)
            const float u1 = std::pow(std::sin(phase1 * static_cast<float>(M_PI)), 2.0f);
            const float u2 = std::pow(std::sin(phase2 * static_cast<float>(M_PI)), 2.0f);
            
            const float P1 = 3.0f * u1 * u1 - 2.0f * u1 * u1 * u1;
            const float P2 = 3.0f * u2 * u2 - 2.0f * u2 * u2 * u2;
            
            const float gain1 = std::sqrt(P1);
            const float gain2 = std::sqrt(P2);

            // 6. Read from delay lines
            double readPos1 = static_cast<double>(m_writePos) - delay1;
            if (readPos1 < 0.0) readPos1 += m_bufferSize;
            
            double readPos2 = static_cast<double>(m_writePos) - delay2;
            if (readPos2 < 0.0) readPos2 += m_bufferSize;

            const float out1 = readInterpolated(readPos1);
            const float out2 = readInterpolated(readPos2);

            // 7. Mix the two taps
            float wet = out1 * gain1 + out2 * gain2;

            // 8. Advance write pointer
            m_writePos = (m_writePos + 1) % m_bufferSize;

            // 9. Mix and Bypass smooth
            m_mixLevel = m_smoothCoeff * m_mixLevel + (1.0f - m_smoothCoeff) * m_targetMix;
            m_bypassGain = m_bypassSmoothCoeff * m_bypassGain
                          + (1.0f - m_bypassSmoothCoeff) * m_targetBypassGain;
            
            const float finalWetGain = m_mixLevel * m_bypassGain;
            output[i] = finalWetGain * wet + (1.0f - finalWetGain) * input[i];
        }
    }

private:

    // ── Interpolazione Hermite cubica ─────────────────────────────────────────
    inline float hermite(float y0, float y1, float y2, float y3, float t) const
    {
        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + c0;
    }

    float readInterpolated(double pos) const
    {
        const int    i0 = static_cast<int>(pos);
        const float  t  = static_cast<float>(pos - i0);

        const int n = m_bufferSize;
        const float y0 = m_buffer[((i0 - 1) % n + n) % n];
        const float y1 = m_buffer[  i0             % n       ];
        const float y2 = m_buffer[ (i0 + 1)        % n       ];
        const float y3 = m_buffer[ (i0 + 2)        % n       ];

        return hermite(y0, y1, y2, y3, t);
    }

    // ── State ─────────────────────────────────────────────────────────────────
    std::vector<float>  m_buffer;
    std::vector<float>  m_hannWindow;
    int                 m_bufferSize        = 0;
    int                 m_writePos          = 0;

    float               m_phase             = 0.0f; // LFO Phase (0.0 to 1.0)

    float               m_targetPitchRatio  = 1.0f;
    float               m_currentPitchRatio = 1.0f;
    float               m_smoothCoeff       = 0.0f;

    float               m_semitones         = 0.0f;
    float               m_fineTuneCents     = 0.0f;

    float               m_targetMix         = 1.0f;
    float               m_mixLevel          = 1.0f;

    float               m_targetBypassGain  = 1.0f;
    float               m_bypassGain        = 1.0f;
    float               m_bypassSmoothCoeff = 0.0f;

    double              m_sampleRate        = 44100.0;
    bool                m_prepared          = false;
};

} // namespace PitchWrench
