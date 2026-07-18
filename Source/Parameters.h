#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// Parameters.h  —  Pitch Wrench parameter definitions
// ID stabili versionati, range, taper, unità. Zero JUCE.
// ─────────────────────────────────────────────────────────────────────────────

namespace PitchWrench {

// ── Stable parameter indices ──────────────────────────────────────────────────
enum ParamIndex {
    kSemitones = 0,
    kGlide     = 1,
    kEnabled   = 2,
    kUiScale   = 3,
    kNumParams = 4
};

// ── Stable string IDs (for APVTS in the JUCE wrapper) ────────────────────────
namespace ParamID {
    inline constexpr const char* Semitones = "semitones_v1";
    inline constexpr const char* Glide     = "glide_v1";
    inline constexpr const char* Enabled   = "enabled_v1";
    inline constexpr const char* UiScale   = "ui_scale_v1";
}

// ── Ranges ────────────────────────────────────────────────────────────────────
namespace ParamRange {
    inline constexpr float SemitonesMin     = -24.0f;
    inline constexpr float SemitonesMax     =  24.0f;
    inline constexpr float SemitonesDefault =   0.0f;
    inline constexpr float SemitonesStep    =   1.0f;  // snap to integer semitones

    inline constexpr float GlideMin         =   5.0f;
    inline constexpr float GlideMax         = 2000.0f;
    inline constexpr float GlideDefault     =   5.0f;
}

// ── Normalization helpers ─────────────────────────────────────────────────────
inline float normalizeSemitones(float semitones) {
    return (semitones - ParamRange::SemitonesMin) / (ParamRange::SemitonesMax - ParamRange::SemitonesMin);
}
inline float denormalizeSemitones(float normalized) {
    return ParamRange::SemitonesMin + normalized * (ParamRange::SemitonesMax - ParamRange::SemitonesMin);
}

inline float normalizeGlide(float ms) {
    return (ms - ParamRange::GlideMin) / (ParamRange::GlideMax - ParamRange::GlideMin);
}
inline float denormalizeGlide(float normalized) {
    return ParamRange::GlideMin + normalized * (ParamRange::GlideMax - ParamRange::GlideMin);
}

} // namespace PitchWrench
