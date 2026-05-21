#pragma once
// =============================================================================
// game_settings.h — + reflectShadows
// =============================================================================

#include "core/key_mapping.h"

enum class ReflectionQuality : int {
    Off     = 0,
    Quarter = 1,
    Half    = 2,
    Full    = 3,
};

inline const char* reflectionQualityName(ReflectionQuality q) {
    switch (q) {
        case ReflectionQuality::Off:     return "Off";
        case ReflectionQuality::Quarter: return "1/4";
        case ReflectionQuality::Half:    return "1/2";
        case ReflectionQuality::Full:    return "Full";
    }
    return "Half";
}

inline float reflectionQualityScale(ReflectionQuality q) {
    switch (q) {
        case ReflectionQuality::Off:     return 0.f;
        case ReflectionQuality::Quarter: return 0.25f;
        case ReflectionQuality::Half:    return 0.5f;
        case ReflectionQuality::Full:    return 1.0f;
    }
    return 0.5f;
}


enum class TonemapMode : int {
    ACES        = 0,
    AgX         = 1,
    PBRNeutral  = 2,
};

inline const char* tonemapModeName(TonemapMode m) {
    switch (m) {
        case TonemapMode::ACES:       return "ACES";
        case TonemapMode::AgX:        return "AgX";
        case TonemapMode::PBRNeutral: return "Khronos PBR";
    }
    return "ACES";
}

inline const char* shadowQualityName(int q) {
    switch (q) {
        case 0:  return "Off";
        case 2:  return "High";
        default: return "Soft";
    }
}

struct GameSettings {
    float bgmVolume        = 0.5f;
    float sfxVolume        = 1.0f;
    float mouseSensitivity = 1.0f;

    // ─── Graphics ────────────────────────────────────────
    float drawDistance     = 100.0f;  // m
    ReflectionQuality reflectionQuality = ReflectionQuality::Half;
    bool reflectShadows = false;  // 反射描画でも影を計算するか (重い)
    bool grassWind = true;        // Phase 1F: grass wind sway (toggle)
    int shadowQuality = 1;        // Phase 1G: 0=hard, 1=PCF3x3, 2=PCF5x5
    bool bloom = true;            // Phase 1I: HDR bloom on/off
    TonemapMode tonemapMode = TonemapMode::ACES;

    KeyMapping keyMapping;

    bool keyMappingDirty = false;
    bool persistDirty = false;
    bool reflectionDirty = false;

    static constexpr float kMinVolume       = 0.0f;
    static constexpr float kMaxVolume       = 1.0f;
    static constexpr float kVolumeStep      = 0.05f;
    static constexpr float kMinSensitivity  = 0.1f;
    static constexpr float kMaxSensitivity  = 3.0f;
    static constexpr float kSensitivityStep = 0.1f;

    static constexpr float kMinDrawDistance   = 50.0f;
    static constexpr float kMaxDrawDistance   = 300.0f;
    static constexpr float kDrawDistanceStep  = 10.0f;
};
