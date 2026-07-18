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
static constexpr float kMaxBufferMs       = 80.0f;    // 80ms max buffer to allow safe searching
static constexpr int   kMaxSampleRate     = 192000;
static constexpr float kSmoothTimeMs      = 8.0f;     // smoothing pitch ratio (ms)
static constexpr float kBypassFadeMs      = 3.0f;     // crossfade bypass (ms)

// WSOLA Parameters
static constexpr float kNominalDelayMs    = 35.0f; 
static constexpr float kMinDelayMs        = 10.0f;
static constexpr float kMaxDelayMs        = 60.0f;
static constexpr float kSearchWindowMs    = 15.0f; // +/- 15ms
static constexpr float kTemplateMs        = 10.0f;
static constexpr float kSpliceMs          = 5.0f;

// ─────────────────────────────────────────────────────────────────────────────
class PitchWrenchDSP {
public:

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    void prepare(double sampleRate, int /*maxBlockSize*/)
    {
        m_sampleRate = sampleRate;

        // Alloca buffer circolare — unica allocazione consentita
        const int bufSize = static_cast<int>(sampleRate * kMaxBufferMs * 0.001f) + 8;
        m_buffer.assign(static_cast<size_t>(bufSize), 0.0f);
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
        
        m_readPos = 0.0;
        m_splicePos = 0.0;
        m_inSplice = false;
        m_splicePhase = 0.0f;

        m_currentPitchRatio = 1.0f;
        m_bypassGain = 1.0f;
        m_mixLevel = m_targetMix;
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
public:
    void process(const float* input, float* output, int numSamples)
    {
        if (!m_prepared) {
            std::memset(output, 0, sizeof(float) * static_cast<size_t>(numSamples));
            return;
        }

        const double nominalDelaySamples = m_sampleRate * static_cast<double>(kNominalDelayMs) * 0.001;
        const double minDelaySamples     = m_sampleRate * static_cast<double>(kMinDelayMs) * 0.001;
        const double maxDelaySamples     = m_sampleRate * static_cast<double>(kMaxDelayMs) * 0.001;
        const double searchWindowSamples = m_sampleRate * static_cast<double>(kSearchWindowMs) * 0.001;
        const int    templateSamples     = static_cast<int>(m_sampleRate * static_cast<double>(kTemplateMs) * 0.001);
        const float  splicePhaseInc      = 1.0f / static_cast<float>(m_sampleRate * static_cast<double>(kSpliceMs) * 0.001);

        for (int i = 0; i < numSamples; ++i) {
            // 1. Write to buffer
            m_buffer[static_cast<size_t>(m_writePos)] = input[i];

            // 2. Smooth pitch ratio
            m_currentPitchRatio = m_smoothCoeff * m_currentPitchRatio
                                 + (1.0f - m_smoothCoeff) * m_targetPitchRatio;

            // 3. Advance read positions
            m_readPos += m_currentPitchRatio;
            if (m_readPos >= m_bufferSize) m_readPos -= m_bufferSize;
            
            if (m_inSplice) {
                m_splicePos += m_currentPitchRatio;
                if (m_splicePos >= m_bufferSize) m_splicePos -= m_bufferSize;
                
                m_splicePhase += splicePhaseInc;
                if (m_splicePhase >= 1.0f) {
                    m_inSplice = false;
                    m_readPos = m_splicePos;
                }
            } else {
                // Check boundaries to trigger splice
                double delay = static_cast<double>(m_writePos) - m_readPos;
                if (delay < 0.0) delay += m_bufferSize;

                if (delay < minDelaySamples || delay > maxDelaySamples) {
                    double targetReadPos = static_cast<double>(m_writePos) - nominalDelaySamples;
                    if (targetReadPos < 0.0) targetReadPos += m_bufferSize;

                    m_splicePos = findBestSplicePos(m_readPos, targetReadPos, searchWindowSamples, templateSamples);
                    m_inSplice = true;
                    m_splicePhase = 0.0f;
                }
            }

            // 4. Read and interpolate
            float out1 = readInterpolated(m_readPos);
            float wet = out1;

            if (m_inSplice) {
                float out2 = readInterpolated(m_splicePos);
                // Hann window crossfade (constant amplitude) for perfectly matched waveforms
                float crossfadeGain = 0.5f * (1.0f - std::cos(m_splicePhase * static_cast<float>(M_PI)));
                wet = out1 * (1.0f - crossfadeGain) + out2 * crossfadeGain;
            }

            // 5. Advance write pointer
            m_writePos = (m_writePos + 1) % m_bufferSize;

            // 6. Mix and Bypass smooth
            m_mixLevel = m_smoothCoeff * m_mixLevel + (1.0f - m_smoothCoeff) * m_targetMix;
            m_bypassGain = m_bypassSmoothCoeff * m_bypassGain
                          + (1.0f - m_bypassSmoothCoeff) * m_targetBypassGain;
            
            const float finalWetGain = m_mixLevel * m_bypassGain;
            output[i] = finalWetGain * wet + (1.0f - finalWetGain) * input[i];
        }
    }

private:

    // ── WSOLA Pattern Matching ───────────────────────────────────────────────
    double findBestSplicePos(double currentReadPos, double targetReadPos, double searchWindowSamples, int templateSamples) const
    {
        double bestPos = targetReadPos;
        float minError = 1e9f;

        const int step = 2; // Decimation per le performance
        const int startOffset = -static_cast<int>(searchWindowSamples);
        const int endOffset = static_cast<int>(searchWindowSamples);

        const int currInt = static_cast<int>(currentReadPos);
        const int targetInt = static_cast<int>(targetReadPos);
        const int n = m_bufferSize;

        for (int offset = startOffset; offset <= endOffset; offset += step) {
            int candInt = targetInt + offset;
            float error = 0.0f;

            // Controlliamo il passato (k > 0 va indietro nel tempo)
            // Entrambe le testine hanno registrato questo audio.
            for (int k = 0; k < templateSamples; k += step) {
                int idxCurr = (currInt - k) % n;
                if (idxCurr < 0) idxCurr += n;
                
                int idxCand = (candInt - k) % n;
                if (idxCand < 0) idxCand += n;

                error += std::abs(m_buffer[static_cast<size_t>(idxCurr)] - m_buffer[static_cast<size_t>(idxCand)]);
                if (error >= minError) break; // early exit optimization
            }

            if (error < minError) {
                minError = error;
                bestPos = static_cast<double>(candInt);
            }
        }
        
        // Refine di precisione attorno al punto migliore
        int refineStart = static_cast<int>(bestPos) - step;
        int refineEnd = static_cast<int>(bestPos) + step;
        
        for (int candInt = refineStart; candInt <= refineEnd; ++candInt) {
            float error = 0.0f;
            for (int k = 0; k < templateSamples; ++k) {
                int idxCurr = (currInt - k) % n;
                if (idxCurr < 0) idxCurr += n;
                
                int idxCand = (candInt - k) % n;
                if (idxCand < 0) idxCand += n;

                error += std::abs(m_buffer[static_cast<size_t>(idxCurr)] - m_buffer[static_cast<size_t>(idxCand)]);
                if (error >= minError) break;
            }
            if (error < minError) {
                minError = error;
                bestPos = static_cast<double>(candInt);
            }
        }

        // Mantieni bestPos nel range del buffer
        bestPos = std::fmod(bestPos, static_cast<double>(n));
        if (bestPos < 0.0) bestPos += n;

        return bestPos;
    }

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
        const float y0 = m_buffer[static_cast<size_t>(((i0 - 1) % n + n) % n)];
        const float y1 = m_buffer[static_cast<size_t>(  i0             % n       )];
        const float y2 = m_buffer[static_cast<size_t>( (i0 + 1)        % n       )];
        const float y3 = m_buffer[static_cast<size_t>( (i0 + 2)        % n       )];

        return hermite(y0, y1, y2, y3, t);
    }

    // ── State ─────────────────────────────────────────────────────────────────
    std::vector<float>  m_buffer;
    int                 m_bufferSize        = 0;
    int                 m_writePos          = 0;

    double              m_readPos           = 0.0;
    double              m_splicePos         = 0.0;
    bool                m_inSplice          = false;
    float               m_splicePhase       = 0.0f;

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
