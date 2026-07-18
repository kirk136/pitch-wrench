#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// Parameters.h  —  Pitch Wrench parameter definitions
// ID stabili versionati, range, taper, unità. Zero JUCE.
// ─────────────────────────────────────────────────────────────────────────────

namespace PitchWrench {

// ── Stable parameter indices ──────────────────────────────────────────────────
enum ParamIndex {
    kSemitones = 0,
    kFineTune  = 1,
    kMix       = 2,
    kEnabled   = 3,
    kUiScale   = 4,
    kNumParams = 5
};

// ── Stable string IDs (for APVTS in the JUCE wrapper) ────────────────────────
namespace ParamID {
    inline constexpr const char* Semitones = "semitones_v1";
    inline constexpr const char* FineTune  = "finetune_v1";
    inline constexpr const char* Mix       = "mix_v1";
    inline constexpr const char* Enabled   = "enabled_v1";
    inline constexpr const char* UiScale   = "ui_scale_v1";
}

// ── Ranges ────────────────────────────────────────────────────────────────────
namespace ParamRange {
    inline constexpr float SemitonesMin     = -24.0f;
    inline constexpr float SemitonesMax     =  24.0f;
    inline constexpr float SemitonesDefault =   0.0f;
    inline constexpr float SemitonesStep    =   1.0f;  // snap to integer semitones

    inline constexpr float FineTuneMin      = -100.0f;
    inline constexpr float FineTuneMax      =  100.0f;
    inline constexpr float FineTuneDefault  =    0.0f;

    inline constexpr float MixMin           =   0.0f;
    inline constexpr float MixMax           =   1.0f;
    inline constexpr float MixDefault       =   1.0f;
}

// ── Normalization helpers ─────────────────────────────────────────────────────
inline float normalizeSemitones(float semitones) {
    return (semitones - ParamRange::SemitonesMin) / (ParamRange::SemitonesMax - ParamRange::SemitonesMin);
}
inline float denormalizeSemitones(float normalized) {
    return ParamRange::SemitonesMin + normalized * (ParamRange::SemitonesMax - ParamRange::SemitonesMin);
}

inline float normalizeFineTune(float cents) {
    return (cents - ParamRange::FineTuneMin) / (ParamRange::FineTuneMax - ParamRange::FineTuneMin);
}
inline float denormalizeFineTune(float normalized) {
    return ParamRange::FineTuneMin + normalized * (ParamRange::FineTuneMax - ParamRange::FineTuneMin);
}

// Mix is naturally 0..1, but we add functions for consistency if needed.
inline float normalizeMix(float mix) { return mix; }
inline float denormalizeMix(float normalized) { return normalized; }

} // namespace PitchWrench
