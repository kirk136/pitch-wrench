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
static constexpr int   kMaxBufferSeconds  = 1;        // 1s a 192kHz = abbondante
static constexpr int   kMaxSampleRate     = 192000;
static constexpr int   kMaxBufferSamples  = kMaxBufferSeconds * kMaxSampleRate;
static constexpr int   kCrossfadeMin      = 32;       // campioni — 0.7ms @ 44.1kHz
static constexpr int   kCrossfadeMax      = 256;      // campioni — 5.8ms @ 44.1kHz
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
        const int bufSize = static_cast<int>(sampleRate * kMaxBufferSeconds) + 8;
        m_buffer.assign(bufSize, 0.0f);
        m_bufferSize = bufSize;

        // Calcola dimensione crossfade adattiva al sample rate
        // ~3ms, clamped tra kCrossfadeMin e kCrossfadeMax
        m_crossfadeLength = std::clamp(
            static_cast<int>(sampleRate * 0.003),
            kCrossfadeMin, kCrossfadeMax);

        // Pre-calcola finestra Hann per il crossfade
        m_hannWindow.resize(m_crossfadeLength);
        for (int i = 0; i < m_crossfadeLength; ++i) {
            const float phase = static_cast<float>(i) / (m_crossfadeLength - 1);
            m_hannWindow[i] = 0.5f * (1.0f - std::cos(static_cast<float>(M_PI) * phase));
        }

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

        // Le due testine partono con una distanza pari a metà buffer
        // così non entrano mai in collisione al reset
        m_readPosA = 0.0;
        m_readPosB = static_cast<double>(m_bufferSize / 2);

        m_gainA = 1.0f;   // testa A è quella attiva all'inizio
        m_gainB = 0.0f;
        m_activeHead = 0;
        m_crossfadeCounter = 0;
        m_inCrossfade = false;

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

        for (int i = 0; i < numSamples; ++i) {
            // 1. Scrivi campione corrente nel buffer circolare
            m_buffer[m_writePos] = input[i];

            // 2. Smooth del pitch ratio (one-pole, realtime safe)
            m_currentPitchRatio = m_smoothCoeff * m_currentPitchRatio
                                 + (1.0f - m_smoothCoeff) * m_targetPitchRatio;

            // 3. Leggi le due testine con interpolazione Hermite cubica
            const float outA = readInterpolated(m_readPosA);
            const float outB = readInterpolated(m_readPosB);

            // 4. Mix pesato (crossfade o steady-state)
            float wet = m_gainA * outA + m_gainB * outB;

            // 5. Aggiorna crossfade se in corso
            if (m_inCrossfade) {
                updateCrossfade();
            }

            // 6. Avanza le testine di lettura (velocità = pitchRatio)
            m_readPosA += m_currentPitchRatio;
            m_readPosB += m_currentPitchRatio;

            // 7. Wrap-around circolare
            while (m_readPosA >= m_bufferSize) m_readPosA -= m_bufferSize;
            while (m_readPosA <  0.0)          m_readPosA += m_bufferSize;
            while (m_readPosB >= m_bufferSize) m_readPosB -= m_bufferSize;
            while (m_readPosB <  0.0)          m_readPosB += m_bufferSize;

            // 8. Avanza write pointer
            m_writePos = (m_writePos + 1) % m_bufferSize;

            // 9. Controlla se serve un nuovo crossfade
            checkCollision();

            // 10. Mix and Bypass smooth (crossfade dry/wet per enable/disable)
            m_mixLevel = m_smoothCoeff * m_mixLevel + (1.0f - m_smoothCoeff) * m_targetMix;
            m_bypassGain = m_bypassSmoothCoeff * m_bypassGain
                          + (1.0f - m_bypassSmoothCoeff) * m_targetBypassGain;
            
            const float finalWetGain = m_mixLevel * m_bypassGain;
            output[i] = finalWetGain * wet + (1.0f - finalWetGain) * input[i];
        }
    }

private:

    // ── Interpolazione Hermite cubica ─────────────────────────────────────────
    // Migliore qualità di interpolazione lineare, più leggera del sinc.
    // Ideale per pitch shifting di segnali chitarra/basso.
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

        // Leggi 4 campioni con wrap circolare (costanti, no branch variabile)
        const int n = m_bufferSize;
        const float y0 = m_buffer[((i0 - 1) % n + n) % n];
        const float y1 = m_buffer[  i0             % n       ];
        const float y2 = m_buffer[ (i0 + 1)        % n       ];
        const float y3 = m_buffer[ (i0 + 2)        % n       ];

        return hermite(y0, y1, y2, y3, t);
    }

    // ── Logica di collisione e crossfade ──────────────────────────────────────
    //
    // "Collisione" = la testina attiva si avvicina troppo al write pointer.
    // Zona di pericolo: meno di (crossfadeLength * 2) campioni di distanza.
    // Quando avviene, triggeriamo il crossfade verso l'altra testina.

    void checkCollision()
    {
        if (m_inCrossfade) return;  // già in crossfade, aspetta che finisca

        const double activePos = (m_activeHead == 0) ? m_readPosA : m_readPosB;

        // Distanza dalla testina attiva al write pointer (circolare)
        double dist = static_cast<double>(m_writePos) - activePos;
        if (dist < 0.0) dist += m_bufferSize;

        // Zona di pericolo: collisione imminente (pitch up) o sorpasso (pitch down)
        const double dangerZone = m_crossfadeLength * 2.0;

        bool collisionIminent = false;

        if (m_currentPitchRatio > 1.0f) {
            // Pitch UP: la testina si avvicina velocemente al write pointer
            // Collisione se dist < dangerZone
            collisionIminent = (dist < dangerZone);
        } else if (m_currentPitchRatio < 1.0f) {
            // Pitch DOWN: la testina rallenta, il write pointer la supera
            // Collisione se dist > bufferSize - dangerZone
            collisionIminent = (dist > m_bufferSize - dangerZone);
        }

        if (collisionIminent) {
            triggerCrossfade();
        }
    }

    void triggerCrossfade()
    {
        // Riposiziona la testina inattiva a metà buffer di distanza
        // dalla testina attiva, nel punto "sicuro"
        if (m_activeHead == 0) {
            // A è attiva, riposiziona B
            m_readPosB = m_readPosA + static_cast<double>(m_bufferSize / 2);
            while (m_readPosB >= m_bufferSize) m_readPosB -= m_bufferSize;
        } else {
            // B è attiva, riposiziona A
            m_readPosA = m_readPosB + static_cast<double>(m_bufferSize / 2);
            while (m_readPosA >= m_bufferSize) m_readPosA -= m_bufferSize;
        }

        m_inCrossfade = true;
        m_crossfadeCounter = 0;
    }

    void updateCrossfade()
    {
        const int n = m_crossfadeCounter;
        const int L = m_crossfadeLength;

        if (n >= L) {
            // Crossfade completato: switcha la testa attiva
            m_inCrossfade = false;
            m_crossfadeCounter = 0;
            m_activeHead = 1 - m_activeHead;
            // Assicura gain puliti
            m_gainA = (m_activeHead == 0) ? 1.0f : 0.0f;
            m_gainB = (m_activeHead == 1) ? 1.0f : 0.0f;
            return;
        }

        // Hann window: la testa che entra sale, quella che esce scende
        const float fadeIn  = m_hannWindow[n];
        const float fadeOut = m_hannWindow[L - 1 - n];

        if (m_activeHead == 0) {
            // A è attiva e sta uscendo, B sta entrando
            m_gainA = fadeOut;
            m_gainB = fadeIn;
        } else {
            // B è attiva e sta uscendo, A sta entrando
            m_gainB = fadeOut;
            m_gainA = fadeIn;
        }

        ++m_crossfadeCounter;
    }

    // ── State ─────────────────────────────────────────────────────────────────
    std::vector<float>  m_buffer;
    std::vector<float>  m_hannWindow;
    int                 m_bufferSize        = 0;
    int                 m_writePos          = 0;

    double              m_readPosA          = 0.0;
    double              m_readPosB          = 0.0;
    float               m_gainA             = 1.0f;
    float               m_gainB             = 0.0f;
    int                 m_activeHead        = 0;    // 0=A, 1=B
    bool                m_inCrossfade       = false;
    int                 m_crossfadeCounter  = 0;
    int                 m_crossfadeLength   = 64;

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
