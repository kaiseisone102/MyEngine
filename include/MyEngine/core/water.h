#pragma once
// =============================================================================
// core/water.h — 水面の component と draw params
// =============================================================================
// CWater: entity に付ける component。 mesh ポインタは WorldWater が管理。
// WaterDrawParams: shader push constants 用パラメータ。
// =============================================================================

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

class WaterMesh;

struct WaterDrawParams {
    glm::vec3 shallowColor = {0.30f, 0.55f, 0.70f};
    glm::vec3 deepColor    = {0.05f, 0.15f, 0.30f};
    float waveAmp          = 0.08f;
    float waveSpeed        = 1.0f;
    float waveWavelength   = 2.5f;
    float fresnelPower     = 5.0f;
    float specularPower    = 64.0f;
    float baseAlpha        = 0.75f;
};

struct CWater {
    glm::vec3 center{0.f, 0.f, 0.f};
    glm::vec2 sizeXZ{16.f, 16.f};
    WaterMesh* mesh = nullptr;  // WorldWater が管理
    WaterDrawParams drawParams;
};

struct WaterTag {};
